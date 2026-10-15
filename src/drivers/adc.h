/*
 * adc.h — ADC del RP2040 por registros (12 bits, AIN0..AIN2 = GP26..GP28).
 * AIN3 = GP29 lo usa el Wi-Fi (VSYS/3) y AIN4 es el sensor de temperatura.
 */
#ifndef ADC_H
#define ADC_H

#include <stdint.h>

void adc_iniciar(void);

/* Una conversión del canal indicado (0..2). Bloquea ≈ 2 µs. */
uint16_t adc_leer_canal(uint32_t canal);

/* Promedio de n conversiones. */
uint16_t adc_leer_promedio(uint32_t canal, uint32_t n);

#endif /* ADC_H */
