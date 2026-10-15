#include "aplicacion/carro.h"

#include <stdint.h>
#include "configuracion/motor.h"
#include "dominio/limites.h"
#include "drivers/finales.h"
#include "drivers/motor.h"
#include "hw/tiempo.h"

static int           pedida;
static carro_error_t error;
static bool          bloqueado;
static bool          en_movimiento;
static uint32_t      t_inicio_mov;
static uint32_t      timeout_ms = CARRO_TIMEOUT_MS;

static void fallar(carro_error_t e)
{
    error = e;
    pedida = 0;
    en_movimiento = false;
    motor_frenar();
}

void carro_iniciar(void)
{
    motor_iniciar();
    finales_iniciar();
    pedida = 0;
    error = CARRO_ERROR_NINGUNO;
    bloqueado = false;
    en_movimiento = false;

    if (limites_incoherentes(final_inicio_activo(), final_fin_activo())) {
        error = CARRO_ERROR_FINALES_INCOHERENTES;
    }
}

bool carro_armar(bool armar)
{
    if (armar && error != CARRO_ERROR_NINGUNO) {
        return false;
    }
    if (!armar) {
        pedida = 0;
        en_movimiento = false;
    }
    motor_habilitar(armar);
    return true;
}

bool carro_armado(void)
{
    return motor_habilitado();
}

void carro_mover(int velocidad)
{
    if (error != CARRO_ERROR_NINGUNO || !motor_habilitado()) {
        pedida = 0;
        return;
    }
    if (velocidad > 100)  { velocidad = 100; }
    if (velocidad < -100) { velocidad = -100; }
    pedida = velocidad;
}

void carro_parar(void)
{
    pedida = 0;
}

void carro_frenar(void)
{
    pedida = 0;
    en_movimiento = false;
    motor_frenar();
}

void carro_timeout_ms(uint32_t ms)
{
    timeout_ms = ms;
}

void carro_actualizar(void)
{
    finales_actualizar();

    bool ini = final_inicio_activo();
    bool fin = final_fin_activo();

    if (error == CARRO_ERROR_NINGUNO && limites_incoherentes(ini, fin)) {
        fallar(CARRO_ERROR_FINALES_INCOHERENTES);
    }

    if (error == CARRO_ERROR_NINGUNO) {
        int permitida = limites_filtrar(pedida, ini, fin);
        bloqueado = (pedida != 0 && permitida == 0);

        if (permitida == 0 && motor_velocidad_real() != 0 && bloqueado) {
            /* Extremo alcanzado en movimiento: freno inmediato, sin rampa. */
            motor_frenar();
        }

        motor_velocidad(permitida * MOTOR_SIGNO_HACIA_FIN);

        if (permitida != 0) {
            uint32_t ahora = tiempo_us();
            if (!en_movimiento) {
                en_movimiento = true;
                t_inicio_mov = ahora;
            } else if ((uint32_t)(ahora - t_inicio_mov) >= timeout_ms * 1000u) {
                fallar(CARRO_ERROR_TIMEOUT);
            }
        } else {
            en_movimiento = false;
        }
    }

    motor_actualizar();
}

carro_error_t carro_error(void)
{
    return error;
}

const char *carro_error_texto(carro_error_t e)
{
    switch (e) {
    case CARRO_ERROR_NINGUNO:             return "ninguno";
    case CARRO_ERROR_FINALES_INCOHERENTES: return "finales de carrera incoherentes (ambos activos)";
    case CARRO_ERROR_TIMEOUT:             return "tiempo maximo de movimiento excedido";
    }
    return "?";
}

bool carro_reiniciar_error(void)
{
    finales_actualizar();
    if (limites_incoherentes(final_inicio_activo(), final_fin_activo())) {
        return false;
    }
    error = CARRO_ERROR_NINGUNO;
    pedida = 0;
    en_movimiento = false;
    bloqueado = false;
    return true;
}

int carro_velocidad_real(void)
{
    return motor_velocidad_real() * MOTOR_SIGNO_HACIA_FIN;
}

bool carro_bloqueado_por_final(void)
{
    return bloqueado;
}
