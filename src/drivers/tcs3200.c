/*
 * tcs3200.c — Medición de frecuencia del TCS3200 con el contador de una
 * slice PWM (registros PWM, SIO y TIMER).
 */
#include "drivers/tcs3200.h"

#include "configuracion/pines.h"
#include "configuracion/tcs3200.h"
#include "hw/pin.h"
#include "hw/rp2040_regs.h"
#include "hw/tiempo.h"

#define TCS_SLICE       RP_PWM_SLICE(PIN_TCS_OUT)
#define TCS_SLICE_BIT   (1u << TCS_SLICE)

/* S2/S3 por filtro (datasheet TCS3200, tabla 1) */
static const struct { bool s2, s3; } filtros[] = {
    [TCS_FILTRO_ROJO]  = { false, false },
    [TCS_FILTRO_AZUL]  = { false, true  },
    [TCS_FILTRO_CLARO] = { true,  false },
    [TCS_FILTRO_VERDE] = { true,  true  },
};

void tcs3200_iniciar(void)
{
    pin_salida(PIN_TCS_S2, false);
    pin_salida(PIN_TCS_S3, false);

    rp_liberar_reset(RP_RESET_PWM);

    rp_pwm_slice_t *s = &RP_PWM->slice[TCS_SLICE];
    s->csr = RP_PWM_CSR_DIVMODE(RP_PWM_DIVMODE_FLANCO_SUBE);   /* EN = 0 */
    s->div = RP_PWM_DIV(1, 0);  /* 1 cuenta por flanco (INT = 0 significaría 256) */
    s->top = 0xFFFFu;
    s->ctr = 0;

    /* OUT del sensor es push-pull: sin pull interno. */
    pin_pull(PIN_TCS_OUT, PIN_SIN_PULL);
    pin_funcion(PIN_TCS_OUT, RP_FUNC_PWM);
}

static void (*hook_espera)(void);

void tcs3200_poner_espera(void (*hook)(void))
{
    hook_espera = hook;
}

/* Espera us microsegundos llamando al hook de espera. */
static void esperar_con_hook(uint32_t us)
{
    uint32_t t0 = tiempo_us();
    while ((uint32_t)(tiempo_us() - t0) < us) {
        if (hook_espera) {
            hook_espera();
        }
    }
}

uint32_t tcs3200_medir_canal(tcs_filtro_t filtro, bool *desborde)
{
    rp_pwm_slice_t *s     = &RP_PWM->slice[TCS_SLICE];
    rp_pwm_slice_t *s_set = &RP_PWM_SET->slice[TCS_SLICE];
    rp_pwm_slice_t *s_clr = &RP_PWM_CLR->slice[TCS_SLICE];

    pin_escribir(PIN_TCS_S2, filtros[filtro].s2);
    pin_escribir(PIN_TCS_S3, filtros[filtro].s3);
    esperar_con_hook(TCS_ESPERA_FILTRO_US);

    s->ctr = 0;
    RP_PWM->intr = TCS_SLICE_BIT;           /* limpia la marca de vuelta (W1C) */

    uint32_t t0 = tiempo_us();
    s_set->csr = RP_PWM_CSR_EN;             /* abre la ventana */
    while ((uint32_t)(tiempo_us() - t0) < TCS_VENTANA_US) {
        if (hook_espera) {
            hook_espera();
        }
    }
    s_clr->csr = RP_PWM_CSR_EN;             /* cierra la ventana */
    uint32_t dt = tiempo_us() - t0;

    uint32_t cuentas = s->ctr;
    *desborde = (RP_PWM->intr & TCS_SLICE_BIT) != 0;

    /* Hz = cuentas / dt[µs] × 10⁶ — en 64 bits para no desbordar. */
    return (uint32_t)(((uint64_t)cuentas * 1000000u + dt / 2u) / dt);
}

bool tcs3200_medir(tcs_frecuencias_t *f)
{
    bool d_r, d_g, d_b, d_c;
    f->r = tcs3200_medir_canal(TCS_FILTRO_ROJO,  &d_r);
    f->g = tcs3200_medir_canal(TCS_FILTRO_VERDE, &d_g);
    f->b = tcs3200_medir_canal(TCS_FILTRO_AZUL,  &d_b);
    f->c = tcs3200_medir_canal(TCS_FILTRO_CLARO, &d_c);
    return !(d_r || d_g || d_b || d_c);
}
