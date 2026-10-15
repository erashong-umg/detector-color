#include "drivers/adc.h"

#include <stdbool.h>
#include "hw/rp2040_regs.h"

static uint32_t canal_actual = 0xFFu;

void adc_iniciar(void)
{
    rp_liberar_reset(RP_RESET_ADC);

    RP_ADC->cs = RP_ADC_CS_EN;
    while (!(RP_ADC->cs & RP_ADC_CS_READY)) { }
    canal_actual = 0xFFu;
}

static uint16_t convertir(void)
{
    RP_ADC_SET->cs = RP_ADC_CS_START_ONCE;      /* alias SET: no toca el resto de bits */
    while (!(RP_ADC->cs & RP_ADC_CS_READY)) { }
    return (uint16_t)(RP_ADC->result & 0xFFFu);
}

uint16_t adc_leer_canal(uint32_t canal)
{
    if (canal != canal_actual) {
        RP_ADC->cs = (RP_ADC->cs & ~RP_ADC_CS_AINSEL_MASK) | (canal << RP_ADC_CS_AINSEL_LSB);
        canal_actual = canal;
        (void)convertir();      /* descarta la primera: el condensador de muestreo
                                   aún guarda la tensión del canal anterior */
    }
    return convertir();
}

uint16_t adc_leer_promedio(uint32_t canal, uint32_t n)
{
    uint32_t suma = 0;
    for (uint32_t i = 0; i < n; i++) {
        suma += adc_leer_canal(canal);
    }
    return (uint16_t)(suma / n);
}
