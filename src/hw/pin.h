/*
 * pin.h — Acceso a GPIO mediante las estructuras de registros (rp2040_regs.h).
 *
 * Un pin se configura en dos bloques:
 *   PADS_BANK0  parte eléctrica: entrada habilitada, pull-up/down, drive
 *   IO_BANK0    multiplexor: qué periférico controla el pin (FUNCSEL)
 * y, si es GPIO por software (FUNCSEL = SIO), se lee/escribe en SIO.
 */
#ifndef PIN_H
#define PIN_H

#include <stdbool.h>
#include <stdint.h>
#include "rp2040_regs.h"

typedef enum {
    PIN_SIN_PULL   = 0,
    PIN_PULL_UP    = RP_PAD_PUE,
    PIN_PULL_DOWN  = RP_PAD_PDE
} pin_pull_t;

/* Saca IO_BANK0 y PADS_BANK0 del reset. */
void pin_inicializar(void);

/* Conecta el pin a un periférico (SPI, PWM, SIO...) con entrada habilitada. */
void pin_funcion(uint32_t pin, rp_funcion_t funcion);

/* Configura el pull interno del pad. */
void pin_pull(uint32_t pin, pin_pull_t pull);

/* GPIO por software. */
void pin_salida(uint32_t pin, bool nivel_inicial);
void pin_entrada(uint32_t pin, pin_pull_t pull);

/* Desconecta la parte digital del pad para usarlo como entrada ADC (GPIO26..29). */
void pin_analogico(uint32_t pin);

static inline void pin_alto(uint32_t pin)     { RP_SIO->gpio_out_set = 1u << pin; }
static inline void pin_bajo(uint32_t pin)     { RP_SIO->gpio_out_clr = 1u << pin; }
static inline void pin_alternar(uint32_t pin) { RP_SIO->gpio_out_xor = 1u << pin; }
static inline void pin_escribir(uint32_t pin, bool nivel)
{
    if (nivel) pin_alto(pin); else pin_bajo(pin);
}
static inline bool pin_leer(uint32_t pin)
{
    return (RP_SIO->gpio_in >> pin) & 1u;
}

#endif /* PIN_H */
