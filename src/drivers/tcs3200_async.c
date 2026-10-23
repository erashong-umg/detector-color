/*
 * tcs3200_async.c — Adquisición del TCS3200 por la interrupción de ALARM0 (ver .h).
 */
#include "drivers/tcs3200_async.h"

#include "configuracion/pines.h"
#include "configuracion/tcs3200.h"
#include "hw/ciclos.h"
#include "hw/pin.h"
#include "hw/rp2040_regs.h"

#define TCS_SLICE       RP_PWM_SLICE(PIN_TCS_OUT)
#define TCS_SLICE_BIT   (1u << TCS_SLICE)
#define NUM_CANALES     4u
#define NUM_PC          8u

typedef enum { FASE_ESPERA, FASE_CUENTA } fase_t;

/* Orden de medición y niveles de S2/S3 (datasheet TCS3200, tabla 1). */
static const struct { tcs_filtro_t filtro; bool s2, s3; } canales[NUM_CANALES] = {
    { TCS_FILTRO_ROJO,  false, false },
    { TCS_FILTRO_VERDE, true,  true  },
    { TCS_FILTRO_AZUL,  false, true  },
    { TCS_FILTRO_CLARO, true,  false },
};

/* Juego de medidas en bruto: cuentas del contador y duración real de cada ventana. */
typedef struct {
    uint32_t cuentas[NUM_CANALES];
    uint32_t dt_us[NUM_CANALES];
    bool     desborde;
} juego_t;

/* Estado interno de la ISR (solo la ISR lo toca). */
static fase_t   fase;
static uint32_t canal;
static uint32_t t_inicio_us;
static juego_t  en_curso;

/* Estado compartido con el bucle principal. 'secuencia' hace de cerrojo de secuencia:
   impar mientras la ISR escribe 'publicado', par cuando es coherente. */
static volatile juego_t   publicado;
static volatile uint32_t  secuencia;
static uint32_t           secuencia_leida;

static volatile uint32_t  n_interrupciones;
static volatile uint32_t  n_juegos;
static volatile uint32_t  isr_max_ciclos;
static volatile uint32_t  pcs[NUM_PC];
static volatile uint32_t  pcs_n;

static inline void programar_alarma(uint32_t en_us)
{
    RP_TIMER->alarm[0] = RP_TIMER->timerawl + en_us;     /* escribir ALARM0 la arma */
}

static inline void elegir_filtro(uint32_t c)
{
    pin_escribir(PIN_TCS_S2, canales[c].s2);
    pin_escribir(PIN_TCS_S3, canales[c].s3);
}

void tcs3200_async_iniciar(void)
{
    tcs3200_iniciar();                      /* slice PWM2 en modo contador de flancos, GP5 */

    pin_salida(PIN_DEP_ISR, false);

    canal = 0;
    fase  = FASE_ESPERA;
    en_curso.desborde = false;
    secuencia_leida = secuencia;            /* un reinicio no entrega el juego de la vez anterior */
    elegir_filtro(0);

    RP_TIMER->intr = 1u;                    /* limpia una posible bandera vieja de ALARM0 (W1C) */
    RP_TIMER_SET->inte = 1u;                /* TIMER.INTE bit 0: ALARM0 genera interrupción */

    RP_NVIC->icpr = 1u << RP_IRQ_TIMER_0;   /* quita una IRQ pendiente */
    RP_NVIC->ipr[0] = (RP_NVIC->ipr[0] & ~0xFFu) | (1u << 6);   /* IRQ0: prioridad 1 de 0..3 (bits 7:6) */
    RP_NVIC->iser = 1u << RP_IRQ_TIMER_0;   /* habilita la IRQ en el NVIC */

    programar_alarma(TCS_ESPERA_FILTRO_US);
}

void tcs3200_async_detener(void)
{
    RP_NVIC->icer = 1u << RP_IRQ_TIMER_0;
    RP_TIMER_CLR->inte = 1u;
    RP_PWM_CLR->slice[TCS_SLICE].csr = RP_PWM_CSR_EN;
    RP_TIMER->intr = 1u;
}

/* Cuerpo de la ISR. Recibe el marco de excepción que apiló el hardware:
   {r0, r1, r2, r3, r12, lr, pc, xpsr}; marco[6] es la instrucción donde estaba el programa. */
void tcs3200_async_isr(const uint32_t *marco);
void tcs3200_async_isr(const uint32_t *marco)
{
    uint32_t c0 = ciclos_leer();
    pin_alto(PIN_DEP_ISR);

    RP_TIMER->intr = 1u;                    /* baja la bandera de ALARM0 (W1C) */
    pcs[pcs_n++ % NUM_PC] = marco[6];
    n_interrupciones++;

    rp_pwm_slice_t *s = &RP_PWM->slice[TCS_SLICE];

    if (fase == FASE_ESPERA) {
        /* El filtro ya se asentó: abrir la ventana de conteo. */
        s->ctr = 0;
        RP_PWM->intr = TCS_SLICE_BIT;       /* limpia la marca de vuelta del contador (W1C) */
        t_inicio_us = RP_TIMER->timerawl;
        RP_PWM_SET->slice[TCS_SLICE].csr = RP_PWM_CSR_EN;
        fase = FASE_CUENTA;
        programar_alarma(TCS_VENTANA_US);
    } else {
        /* Fin de la ventana: cerrar, guardar y pasar al siguiente filtro. */
        RP_PWM_CLR->slice[TCS_SLICE].csr = RP_PWM_CSR_EN;
        uint32_t dt = RP_TIMER->timerawl - t_inicio_us;

        en_curso.cuentas[canal] = s->ctr;
        en_curso.dt_us[canal]   = dt;
        if (RP_PWM->intr & TCS_SLICE_BIT) {
            en_curso.desborde = true;
        }

        canal++;
        if (canal == NUM_CANALES) {
            secuencia++;                    /* impar: escritura en curso */
            for (uint32_t i = 0; i < NUM_CANALES; i++) {
                publicado.cuentas[i] = en_curso.cuentas[i];
                publicado.dt_us[i]   = en_curso.dt_us[i];
            }
            publicado.desborde = en_curso.desborde;
            secuencia++;                    /* par: coherente */
            n_juegos++;
            canal = 0;
            en_curso.desborde = false;
        }
        elegir_filtro(canal);
        fase = FASE_ESPERA;
        programar_alarma(TCS_ESPERA_FILTRO_US);
    }

    pin_bajo(PIN_DEP_ISR);
    uint32_t dur = ciclos_entre(c0, ciclos_leer());
    if (dur > isr_max_ciclos) {
        isr_max_ciclos = dur;
    }
}

/* Punto de entrada que está en la tabla de vectores (entrada 16, TIMER_IRQ_0).
   Es 'naked': no tiene prólogo, así que en este punto SP (MSP) apunta exactamente al marco que
   apiló el hardware. Se lo pasa en r0 a la función C y salta a ella SIN tocar LR, de modo que
   el 'return' de esa función ejecuta el retorno de excepción (LR = EXC_RETURN). */
__attribute__((naked, used)) void isr_irq0(void)
{
    __asm__ volatile (
        "mrs r0, msp\n\t"
        "ldr r1, =tcs3200_async_isr\n\t"
        "bx  r1\n\t"
        ".ltorg\n\t"
    );
}

bool tcs3200_async_tomar(tcs_frecuencias_t *f, bool *valido)
{
    juego_t copia;
    uint32_t s1, s2;

    do {
        s1 = secuencia;
        for (uint32_t i = 0; i < NUM_CANALES; i++) {
            copia.cuentas[i] = publicado.cuentas[i];
            copia.dt_us[i]   = publicado.dt_us[i];
        }
        copia.desborde = publicado.desborde;
        s2 = secuencia;
    } while (s1 != s2 || (s1 & 1u));        /* la ISR escribió mientras se copiaba: repetir */

    if (s1 == secuencia_leida) {
        return false;                       /* nada nuevo desde la última llamada */
    }
    secuencia_leida = s1;

    uint32_t hz[NUM_CANALES];
    for (uint32_t i = 0; i < NUM_CANALES; i++) {
        uint32_t dt = copia.dt_us[i] ? copia.dt_us[i] : 1u;
        hz[i] = (uint32_t)(((uint64_t)copia.cuentas[i] * 1000000u + dt / 2u) / dt);
    }
    f->r = hz[0];
    f->g = hz[1];
    f->b = hz[2];
    f->c = hz[3];
    if (valido) {
        *valido = !copia.desborde;
    }
    return true;
}

uint32_t tcs3200_async_interrupciones(void) { return n_interrupciones; }
uint32_t tcs3200_async_juegos(void)         { return n_juegos; }
uint32_t tcs3200_async_isr_ciclos_max(void) { return isr_max_ciclos; }

uint32_t tcs3200_async_pc_interrumpido(uint32_t i)
{
    return pcs[(pcs_n - 1u - i) % NUM_PC];
}
