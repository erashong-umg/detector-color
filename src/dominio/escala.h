/*
 * escala.h — Mapeo color → posición en la escala cromática, sin hardware.
 */
#ifndef ESCALA_H
#define ESCALA_H

#include <stdbool.h>
#include <stdint.h>
#include "dominio/color.h"

typedef enum {
    ESCALA_CROMATICO,   /* tiene tono: se puede situar en la escala */
    ESCALA_NEGRO,
    ESCALA_BLANCO,
    ESCALA_GRIS
} escala_clase_t;

typedef struct {
    escala_clase_t clase;
    int32_t x_um;       /* posición objetivo en µm (para acromáticos, la política) */
    bool    tiene_posicion; /* false si el cursor no debe moverse */
} escala_t;

/* Posición directa de un tono (0..359) sobre la escala, sin política del rojo. */
int32_t escala_um_de_tono(int h);

/* Clasifica y sitúa un color. actual_um es la posición actual del cursor
   (−1 si se desconoce) y decide el extremo en la banda del rojo. */
escala_t escala_mapear(color_hsv_t hsv, int32_t actual_um);

/* ¿Merece la pena mover el cursor? (fuera de la zona muerta) */
bool escala_hay_que_mover(int32_t actual_um, int32_t objetivo_um);

const char *escala_clase_texto(escala_clase_t c);

#endif /* ESCALA_H */
