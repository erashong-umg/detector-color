/* ADC simulado: el canal 2 (IR de proximidad) devuelve el valor que fija la prueba. */
#include "drivers/adc.h"

int mock_adc_prox = 4095;
int mock_od_nivel = 1;     /* nivel eléctrico de OD que devuelve pin_leer() */

void adc_iniciar(void) { }

uint16_t adc_leer_canal(uint32_t canal)
{
    return canal == 2 ? (uint16_t)mock_adc_prox : 2048;
}

uint16_t adc_leer_promedio(uint32_t canal, uint32_t n)
{
    (void)n;
    return adc_leer_canal(canal);
}
