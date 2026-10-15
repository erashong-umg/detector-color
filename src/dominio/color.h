/*
 * color.h — Cálculos de color, sin dependencias de hardware.
 *
 * Calibración de dos puntos por canal:
 *   negro  = frecuencia con una muestra negra (luz ambiente + corriente oscura)
 *   blanco = frecuencia con una muestra blanca (fija también el balance de blanco)
 *   valor  = (f − negro) × 255 / (blanco − negro), recortado a 0..255
 */
#ifndef COLOR_H
#define COLOR_H

#include <stdbool.h>
#include <stdint.h>

/* Misma forma que tcs_frecuencias_t, pero el dominio no incluye drivers. */
typedef struct {
    uint32_t r, g, b, c;
} color_frecuencias_t;

typedef struct {
    color_frecuencias_t negro;
    color_frecuencias_t blanco;
    bool hay_negro;
    bool hay_blanco;
} color_calibracion_t;

typedef struct {
    uint8_t r, g, b;
} color_rgb_t;

typedef struct {
    int16_t h;      /* tono 0..359, o −1 si no está definido (gris puro) */
    uint8_t s;      /* saturación 0..255 */
    uint8_t v;      /* valor (brillo) 0..255 */
} color_hsv_t;

static inline bool color_calibrado(const color_calibracion_t *cal)
{
    return cal->hay_negro && cal->hay_blanco;
}

/* Devuelve false si falta calibración o si blanco ≤ negro en algún canal. */
bool color_normalizar(const color_calibracion_t *cal,
                      const color_frecuencias_t *f, color_rgb_t *rgb);

/* Escribe "#RRGGBB" (8 bytes con el terminador). */
void color_a_hex(color_rgb_t rgb, char hex[8]);

color_hsv_t color_a_hsv(color_rgb_t rgb);

/* Inverso de color_a_hsv. h se toma módulo 360; si s = 0 sale un gris. */
color_rgb_t color_desde_hsv(color_hsv_t hsv);

#endif /* COLOR_H */
