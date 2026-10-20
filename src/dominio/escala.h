/*
 * escala.h — Mapeo color → temperatura (cálido/frío/neutro) → posición del carrito, sin hardware.
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

typedef enum {
    TEMP_CALIDO,
    TEMP_NEUTRO,        /* sin tono: negro, gris o blanco */
    TEMP_FRIO
} temperatura_t;

typedef struct {
    escala_clase_t clase;
    temperatura_t  temperatura;
    int32_t        x_um;    /* posición de parada del carrito para esa temperatura */
} escala_t;

/* Posición de parada (µm) de cada temperatura. */
int32_t escala_um_de_temperatura(temperatura_t t);

/* Temperatura de un tono (0..359°); h < 0 = sin tono → neutro. */
temperatura_t escala_temperatura_de_tono(int h);

/* Clasifica un color (clase, temperatura) y le asigna su posición. */
escala_t escala_mapear(color_hsv_t hsv);

/* ¿Merece la pena mover el cursor? (fuera de la zona muerta) */
bool escala_hay_que_mover(int32_t actual_um, int32_t objetivo_um);

const char *escala_clase_texto(escala_clase_t c);
const char *escala_temperatura_texto(temperatura_t t);

#endif /* ESCALA_H */
