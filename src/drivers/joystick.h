/*
 * joystick.h — Joystick analógico (JX → ADC0, JY → ADC1).
 * El botón SW es un botón más (BOTON_JOY en botones.h).
 */
#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>

typedef struct {
    int x, y;                   /* −100..+100, 0 dentro de la zona muerta */
    uint16_t crudo_x, crudo_y;  /* lectura ADC, 0..4095 */
} joystick_t;

/* Hace falta adc_iniciar() antes. Mide el centro: NO tocar el joystick. */
void joystick_iniciar(void);

void joystick_leer(joystick_t *j);

/* Centro medido en el arranque (para diagnóstico). */
void joystick_centro(uint16_t *x, uint16_t *y);

#endif /* JOYSTICK_H */
