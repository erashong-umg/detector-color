/*
 * selector.h — Selector de color del modo manual, sin hardware.
 *
 * El joystick no conduce el motor: mueve una marca por la barra cromática. X recorre el tono
 * (0°..360°, dando la vuelta) e Y sube o baja la saturación; con poca saturación el color es
 * un gris, es decir, NEUTRO. El color elegido se clasifica igual que una lectura real
 * (escala_mapear) y el carrito va a la posición de su temperatura: cálido, neutro o frío.
 */
#ifndef SELECTOR_H
#define SELECTOR_H

#include <stdint.h>
#include "dominio/color.h"

typedef struct {
    int32_t h_x1000;    /* tono en milésimas de grado, 0..359999 */
    int     s;          /* saturación 0..255                     */
} selector_t;

/* Empieza en rojo vivo. */
void selector_iniciar(selector_t *sel);

/* Avanza el selector: joy_x (−100..100, continuo) mueve el tono y dir (−1 = arriba, +1 = abajo,
   un pulso por movimiento) sube o baja la saturación. Se llama a intervalos fijos. */
void selector_mover(selector_t *sel, int joy_x, int dir);

/* Color elegido: el tono y la saturación del selector con un brillo fijo. */
color_hsv_t selector_hsv(const selector_t *sel);

#endif /* SELECTOR_H */
