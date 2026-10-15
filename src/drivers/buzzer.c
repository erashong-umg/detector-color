#include "drivers/buzzer.h"

#include "configuracion/actuadores.h"
#include "configuracion/pines.h"
#include "hw/pin.h"
#include "hw/rp2040_regs.h"
#include "hw/tiempo.h"

#define BZ_SLICE   RP_PWM_SLICE(PIN_BUZZER)

_Static_assert(RP_PWM_CANAL_B(PIN_BUZZER) == 1u,
               "buzzer.c: se asume el buzzer en el canal B de la slice");

static const nota_t *secuencia;
static uint8_t       cantidad;
static uint8_t       indice;
static uint32_t      t_fin;
static bool          activo;

/* ------------------------------------------------------------------ */
/* Capa hardware                                                       */
/* ------------------------------------------------------------------ */

static void hw_silencio(void)
{
    /* Duty 0: la salida queda en bajo con la slice habilitada. */
    RP_PWM->slice[BZ_SLICE].cc = 0;
}

static void hw_tono(uint32_t hz)
{
    if (hz == 0) {
        hw_silencio();
        return;
    }
    if (hz < BUZZER_FREC_MIN_HZ) { hz = BUZZER_FREC_MIN_HZ; }
    if (hz > BUZZER_FREC_MAX_HZ) { hz = BUZZER_FREC_MAX_HZ; }

    /* Periodo = TOP + 1 cuentas de CLK_SYS / divisor */
    uint32_t periodo = (CLK_SYS_HZ / BUZZER_DIV_ENTERO + hz / 2u) / hz;
    uint32_t nivel   = periodo * BUZZER_VOLUMEN_PCT / 100u;

    rp_pwm_slice_t *s = &RP_PWM->slice[BZ_SLICE];
    s->top = periodo - 1u;
    s->cc  = nivel << 16;               /* canal B = bits 31:16 */
    if (s->ctr >= periodo) {            /* evita esperar una vuelta entera de 16 bits */
        s->ctr = 0;
    }
}

void buzzer_iniciar(void)
{
    rp_liberar_reset(RP_RESET_PWM);

    rp_pwm_slice_t *s = &RP_PWM->slice[BZ_SLICE];
    s->csr = 0;
    s->div = RP_PWM_DIV(BUZZER_DIV_ENTERO, 0);
    s->top = 0xFFFFu;
    s->ctr = 0;
    s->cc  = 0;
    RP_PWM_SET->slice[BZ_SLICE].csr = RP_PWM_CSR_EN;

    pin_funcion(PIN_BUZZER, RP_FUNC_PWM);
    activo = false;
}

/* ------------------------------------------------------------------ */
/* Secuenciador                                                        */
/* ------------------------------------------------------------------ */

static void sonar_nota(void)
{
    const nota_t *n = &secuencia[indice];
    hw_tono(n->hz);
    t_fin = tiempo_us() + (uint32_t)n->ms * 1000u;
}

void buzzer_reproducir(const sonido_t *sonido)
{
    secuencia = sonido->notas;
    cantidad  = sonido->cantidad;
    indice    = 0;
    activo    = cantidad > 0;
    if (activo) {
        sonar_nota();
    }
}

void buzzer_tono(uint16_t hz, uint16_t ms)
{
    static nota_t una;
    static sonido_t sonido = { &una, 1 };
    una.hz = hz;
    una.ms = ms;
    buzzer_reproducir(&sonido);
}

void buzzer_silenciar(void)
{
    activo = false;
    hw_silencio();
}

bool buzzer_ocupado(void)
{
    return activo;
}

void buzzer_actualizar(void)
{
    if (!activo || (int32_t)(tiempo_us() - t_fin) < 0) {
        return;
    }
    if (++indice < cantidad) {
        sonar_nota();
    } else {
        buzzer_silenciar();
    }
}
