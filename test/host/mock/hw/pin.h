/* Sustituto de hw/pin.h para compilar en el PC: solo lo que usa drivers/proximidad.c.
   pin_leer() devuelve el nivel eléctrico de OD que fija la prueba (mock_od_nivel). */
#ifndef PIN_H
#define PIN_H

#include <stdbool.h>
#include <stdint.h>

#define RP_ADC_PRIMER_PIN 26u

typedef enum { PIN_SIN_PULL = 0, PIN_PULL_UP = 1, PIN_PULL_DOWN = 2 } pin_pull_t;

extern int mock_od_nivel;

static inline void pin_analogico(uint32_t pin) { (void)pin; }
static inline void pin_entrada(uint32_t pin, pin_pull_t pull) { (void)pin; (void)pull; }
static inline bool pin_leer(uint32_t pin) { (void)pin; return mock_od_nivel != 0; }

#endif
