/* Motor simulado: misma lógica de rampa y de mapeo de duty que drivers/motor.c,
   pero en lugar de escribir registros actualiza el estado que lee la planta. */
#include "drivers/motor.h"
#include "configuracion/motor.h"
#include "hw/tiempo.h"

/* Estado "eléctrico" que la planta física consulta */
int    planta_dir;        /* +1, −1, 0 */
double planta_duty;       /* % */
int    planta_frenado;

static bool habilitado;
static int objetivo, real;
static uint32_t t_rampa;

static unsigned velocidad_a_duty(int v)
{
    if (v < 0) v = -v;
    if (v == 0) return 0;
    if (v > 100) v = 100;
    return MOTOR_DUTY_MIN_PCT + (100u - MOTOR_DUTY_MIN_PCT) * (unsigned)(v - 1) / 99u;
}

static void hw_aplicar(int v)
{
    planta_frenado = 0;
    planta_dir = v > 0 ? +1 : (v < 0 ? -1 : 0);
    planta_duty = velocidad_a_duty(v);
}

void motor_iniciar(void)
{
    habilitado = false; objetivo = real = 0; t_rampa = tiempo_us();
    planta_dir = 0; planta_duty = 0; planta_frenado = 0;
}

void motor_habilitar(bool h)
{
    if (!h) { hw_aplicar(0); objetivo = real = 0; }
    habilitado = h;
}

bool motor_habilitado(void) { return habilitado; }

void motor_velocidad(int v)
{
    if (v > 100) v = 100;
    if (v < -100) v = -100;
    objetivo = habilitado ? v : 0;
}

void motor_frenar(void)
{
    planta_duty = 0; planta_dir = 0; planta_frenado = 1;
    objetivo = real = 0;
}

void motor_actualizar(void)
{
    uint32_t ahora = tiempo_us();
    while ((uint32_t)(ahora - t_rampa) >= MOTOR_RAMPA_MS_PASO * 1000u) {
        t_rampa += MOTOR_RAMPA_MS_PASO * 1000u;
        int destino = objetivo;
        if ((real > 0 && destino < 0) || (real < 0 && destino > 0)) destino = 0;
        if (real == destino) continue;
        int nuevo = real + (destino > real ? 1 : -1);
        if (nuevo != real) { real = nuevo; hw_aplicar(real); }
    }
}

int motor_velocidad_real(void) { return real; }
