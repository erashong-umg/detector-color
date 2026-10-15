/* Sustituto de hw/tiempo.h para compilar en el PC: el reloj es una variable que
   avanza solo cuando el programa "espera". */
#ifndef TIEMPO_H
#define TIEMPO_H

#include <stdint.h>

extern uint32_t mock_us;

static inline uint32_t tiempo_us(void) { return mock_us; }
static inline void tiempo_esperar_us(uint32_t us) { mock_us += us; }
static inline void tiempo_esperar_ms(uint32_t ms) { mock_us += ms * 1000u; }

#endif
