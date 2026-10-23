/*
 * color_rapido.h — Versión optimizada de color.c (Fase 3), sin dependencias de hardware.
 *
 * Misma cadena que color_normalizar() + color_a_hsv(), con punto fijo y sin divisiones por muestra:
 *
 *   Normalización:  v = ((f − negro) · R + 2²³) >> 24,   R = ⌈255 · 2²⁴ / (blanco − negro)⌉
 *       R se calcula UNA vez por calibración (color_rapido_preparar), no por muestra.
 *       (f − negro) · R < 255 · 2²⁴ + rango < 2³², así que cabe en 32 bits (color.c usa 64 bits).
 *
 *   HSV:  s = (Δ · TS[max]) >> 16      TS[m] = ⌈255 · 2¹⁶ / m⌉
 *         h = (|x| · TH[Δ]) >> 16      TH[d] = ⌈60 · 2¹⁶ / d⌉   (x = g−b, b−r o r−g)
 *       Con Δ, max ≤ 255 el error del recíproco es menor que 1/m (o 1/Δ), así que el resultado es
 *       EXACTAMENTE el del cociente entero truncado de color_a_hsv() (se comprueba en el PC con
 *       test/host/test_color_rapido.c recorriendo los 16 777 216 colores RGB).
 *
 * La normalización puede diferir en ±1 de color_normalizar() (redondeo de R).
 */
#ifndef COLOR_RAPIDO_H
#define COLOR_RAPIDO_H

#include <stdbool.h>
#include <stdint.h>

#include "dominio/color.h"

typedef struct {
    uint32_t negro[3];      /* r, g, b */
    uint32_t blanco[3];
    uint32_t recip[3];      /* ⌈255 · 2²⁴ / (blanco − negro)⌉ */
    bool     valida;
} color_rapido_cal_t;

/* Rellena las tablas de recíprocos (1 KB·2 en RAM). Llamar una vez al arrancar. */
void color_rapido_iniciar(void);

/* Precalcula los recíprocos de una calibración. false si falta calibrar o blanco ≤ negro. */
bool color_rapido_preparar(const color_calibracion_t *cal, color_rapido_cal_t *rapida);

color_rgb_t color_rapido_normalizar(const color_rapido_cal_t *cal, const color_frecuencias_t *f);

color_hsv_t color_rapido_a_hsv(color_rgb_t rgb);

/* normalizar + a_hsv */
color_hsv_t color_rapido_procesar(const color_rapido_cal_t *cal, const color_frecuencias_t *f);

#endif /* COLOR_RAPIDO_H */
