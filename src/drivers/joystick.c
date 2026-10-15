#include "drivers/joystick.h"

#include "configuracion/entradas.h"
#include "configuracion/pines.h"
#include "dominio/eje.h"
#include "drivers/adc.h"
#include "hw/pin.h"

static uint16_t centro_x = 2048;
static uint16_t centro_y = 2048;

void joystick_iniciar(void)
{
    pin_analogico(PIN_JOY_X);
    pin_analogico(PIN_JOY_Y);

    centro_x = adc_leer_promedio(JOY_CANAL_X, JOY_MUESTRAS_CAL);
    centro_y = adc_leer_promedio(JOY_CANAL_Y, JOY_MUESTRAS_CAL);
}

void joystick_leer(joystick_t *j)
{
    j->crudo_x = adc_leer_promedio(JOY_CANAL_X, JOY_MUESTRAS);
    j->crudo_y = adc_leer_promedio(JOY_CANAL_Y, JOY_MUESTRAS);
    j->x = eje_normalizar(j->crudo_x, centro_x, JOY_ZONA_MUERTA, JOY_INVERTIR_X);
    j->y = eje_normalizar(j->crudo_y, centro_y, JOY_ZONA_MUERTA, JOY_INVERTIR_Y);
}

void joystick_centro(uint16_t *x, uint16_t *y)
{
    *x = centro_x;
    *y = centro_y;
}
