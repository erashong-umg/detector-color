/*
 * color_ref_float.c — Cadena de color con float (referencia de la Fase 3, ver .h).
 */
#include "dominio/color_ref_float.h"

static uint8_t canal(uint32_t f, uint32_t negro, uint32_t blanco)
{
    if (f <= negro) {
        return 0;
    }
    if (f >= blanco) {
        return 255;
    }
    float x = (float)(f - negro) * 255.0f / (float)(blanco - negro);
    return (uint8_t)(x + 0.5f);
}

color_hsv_t color_flotante_procesar(const color_calibracion_t *cal, const color_frecuencias_t *f)
{
    float r = canal(f->r, cal->negro.r, cal->blanco.r);
    float g = canal(f->g, cal->negro.g, cal->blanco.g);
    float b = canal(f->b, cal->negro.b, cal->blanco.b);

    float max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float delta = max - min;
    color_hsv_t hsv;

    hsv.v = (uint8_t)max;
    hsv.s = max == 0.0f ? 0 : (uint8_t)(delta * 255.0f / max);

    if (delta == 0.0f) {
        hsv.h = -1;
        return hsv;
    }

    float h;
    if (max == r) {
        h = 60.0f * (g - b) / delta;
    } else if (max == g) {
        h = 120.0f + 60.0f * (b - r) / delta;
    } else {
        h = 240.0f + 60.0f * (r - g) / delta;
    }
    if (h < 0.0f) {
        h += 360.0f;
    }
    int hi = (int)h;
    hsv.h = (int16_t)(hi >= 360 ? hi - 360 : hi);
    return hsv;
}
