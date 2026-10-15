#include "drivers/motor.h"

#include "configuracion/motor.h"
#include "configuracion/pines.h"
#include "hw/pin.h"
#include "hw/rp2040_regs.h"
#include "hw/tiempo.h"

#define M_SLICE  RP_PWM_SLICE(PIN_MOTOR_PWMA)

_Static_assert(RP_PWM_CANAL_B(PIN_MOTOR_PWMA) == 0u,
               "motor.c: se asume PWMA en el canal A de la slice");

static bool     habilitado;
static int      objetivo;
static int      real;
static uint32_t t_rampa;

/* ------------------------------------------------------------------ */
/* Capa hardware                                                       */
/* ------------------------------------------------------------------ */

static void hw_duty(uint32_t pct)
{
    /* Canal A = bits 15:0 de CC; el canal B de la slice (GP9 = AIN1) es SIO. */
    RP_PWM->slice[M_SLICE].cc = (MOTOR_PWM_TOP + 1u) * pct / 100u;
}

/* Velocidad 1..100 → ciclo de trabajo entre el mínimo útil y el 100 %. */
static uint32_t velocidad_a_duty(int v)
{
    if (v < 0) { v = -v; }
    if (v == 0) { return 0; }
    if (v > 100) { v = 100; }
    return MOTOR_DUTY_MIN_PCT + (100u - MOTOR_DUTY_MIN_PCT) * (uint32_t)(v - 1) / 99u;
}

static void hw_aplicar(int v)
{
    hw_duty(0);                         /* nunca se cambian los IN con potencia aplicada */
    if (v > 0) {
        pin_alto(PIN_MOTOR_AIN1);
        pin_bajo(PIN_MOTOR_AIN2);
    } else if (v < 0) {
        pin_bajo(PIN_MOTOR_AIN1);
        pin_alto(PIN_MOTOR_AIN2);
    } else {
        pin_bajo(PIN_MOTOR_AIN1);       /* parada libre */
        pin_bajo(PIN_MOTOR_AIN2);
    }
    hw_duty(velocidad_a_duty(v));
}

void motor_iniciar(void)
{
    /* Estado seguro ANTES de tocar el PWM: puente en standby, entradas a 0. */
    pin_salida(PIN_MOTOR_STBY, false);
    pin_salida(PIN_MOTOR_AIN1, false);
    pin_salida(PIN_MOTOR_AIN2, false);

    rp_liberar_reset(RP_RESET_PWM);

    rp_pwm_slice_t *s = &RP_PWM->slice[M_SLICE];
    s->csr = 0;
    s->div = RP_PWM_DIV(1, 0);
    s->top = MOTOR_PWM_TOP;
    s->ctr = 0;
    s->cc  = 0;
    RP_PWM_SET->slice[M_SLICE].csr = RP_PWM_CSR_EN;
    pin_funcion(PIN_MOTOR_PWMA, RP_FUNC_PWM);

    habilitado = false;
    objetivo = real = 0;
    t_rampa = tiempo_us();
}

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

void motor_habilitar(bool h)
{
    if (!h) {
        hw_aplicar(0);
        pin_bajo(PIN_MOTOR_STBY);
        objetivo = real = 0;
    } else {
        pin_alto(PIN_MOTOR_STBY);
    }
    habilitado = h;
}

bool motor_habilitado(void)
{
    return habilitado;
}

void motor_velocidad(int v)
{
    if (v > 100)  { v = 100; }
    if (v < -100) { v = -100; }
    objetivo = habilitado ? v : 0;
}

void motor_frenar(void)
{
    hw_duty(0);
    pin_alto(PIN_MOTOR_AIN1);           /* IN1 = IN2 = H → freno por cortocircuito */
    pin_alto(PIN_MOTOR_AIN2);
    objetivo = real = 0;
}

void motor_actualizar(void)
{
    uint32_t ahora = tiempo_us();

    while ((uint32_t)(ahora - t_rampa) >= MOTOR_RAMPA_MS_PASO * 1000u) {
        t_rampa += MOTOR_RAMPA_MS_PASO * 1000u;

        int destino = objetivo;
        /* Un cambio de sentido pasa por cero antes de invertir. */
        if ((real > 0 && destino < 0) || (real < 0 && destino > 0)) {
            destino = 0;
        }
        if (real == destino) {
            continue;
        }
        int nuevo = real + (destino > real ? 1 : -1);
        if (nuevo != real) {
            real = nuevo;
            hw_aplicar(real);
        }
    }
}

int motor_velocidad_real(void)
{
    return real;
}
