/*
 * eje.h — Normalización de un eje analógico (joystick), sin hardware.
 *
 * El ADC es de 12 bits (0..4095). El centro real del potenciómetro no es
 * 2048 y, con la fuente externa de 3V3, tampoco lo son los extremos, así que
 * se mide el centro en reposo y cada mitad se escala por separado:
 *   raw ≥ centro → 0..+100 sobre (4095 − centro)
 *   raw <  centro → 0..−100 sobre centro
 * La zona muerta se descuenta y el resto se reescala, para que el
 * movimiento arranque en 0 y no dé un salto al salir de ella.
 */
#ifndef EJE_H
#define EJE_H

#include <stdbool.h>
#include <stdint.h>

#define EJE_ADC_MAX  4095

int eje_normalizar(uint16_t crudo, uint16_t centro, int zona_muerta, bool invertir);

#endif /* EJE_H */
