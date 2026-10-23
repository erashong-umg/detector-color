/*
 * color_rapido.c — Normalización y HSV en punto fijo con recíprocos precalculados (ver .h).
 */
#include "dominio/color_rapido.h"

static uint32_t tabla_s[256];   /* TS[m] = ⌈255 · 65536 / m⌉ */
static uint32_t tabla_h[256];   /* TH[d] = ⌈60 · 65536 / d⌉  */

void color_rapido_iniciar(void)
{
    tabla_s[0] = 0;             /* max = 0 → Δ = 0 → s = 0 */
    tabla_h[0] = 0;
    for (uint32_t i = 1; i < 256; i++) {
        tabla_s[i] = (255u * 65536u + i - 1u) / i;
        tabla_h[i] = (60u * 65536u + i - 1u) / i;
    }
}

bool color_rapido_preparar(const color_calibracion_t *cal, color_rapido_cal_t *rapida)
{
    const uint32_t neg[3] = { cal->negro.r,  cal->negro.g,  cal->negro.b  };
    const uint32_t bla[3] = { cal->blanco.r, cal->blanco.g, cal->blanco.b };

    rapida->valida = false;
    if (!color_calibrado(cal)) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (bla[i] <= neg[i]) {
            return false;
        }
        uint32_t rango = bla[i] - neg[i];
        rapida->negro[i]  = neg[i];
        rapida->blanco[i] = bla[i];
        rapida->recip[i]  = (uint32_t)((((uint64_t)255u << 24) + rango - 1u) / rango);
    }
    rapida->valida = true;
    return true;
}

static inline uint8_t canal(uint32_t f, uint32_t negro, uint32_t blanco, uint32_t recip)
{
    if (f <= negro) {
        return 0;
    }
    if (f >= blanco) {
        return 255;
    }
    return (uint8_t)(((f - negro) * recip + (1u << 23)) >> 24);
}

color_rgb_t color_rapido_normalizar(const color_rapido_cal_t *cal, const color_frecuencias_t *f)
{
    color_rgb_t rgb;
    rgb.r = canal(f->r, cal->negro[0], cal->blanco[0], cal->recip[0]);
    rgb.g = canal(f->g, cal->negro[1], cal->blanco[1], cal->recip[1]);
    rgb.b = canal(f->b, cal->negro[2], cal->blanco[2], cal->recip[2]);
    return rgb;
}

color_hsv_t color_rapido_a_hsv(color_rgb_t rgb)
{
    int r = rgb.r, g = rgb.g, b = rgb.b;
    int max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    int min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    uint32_t delta = (uint32_t)(max - min);
    color_hsv_t hsv;

    hsv.v = (uint8_t)max;
    hsv.s = (uint8_t)((delta * tabla_s[max]) >> 16);

    if (delta == 0) {
        hsv.h = -1;
        return hsv;
    }

    int x, base;
    if (max == r) {
        x = g - b;  base = 0;
    } else if (max == g) {
        x = b - r;  base = 120;
    } else {
        x = r - g;  base = 240;
    }

    /* Cociente truncado hacia cero, como la división de C en color_a_hsv(). */
    uint32_t a = (uint32_t)(x < 0 ? -x : x);
    int h = (int)((a * tabla_h[delta]) >> 16);
    if (x < 0) {
        h = -h;
    }
    h += base;
    if (h < 0) {
        h += 360;
    }
    if (h >= 360) {
        h -= 360;
    }
    hsv.h = (int16_t)h;
    return hsv;
}

color_hsv_t color_rapido_procesar(const color_rapido_cal_t *cal, const color_frecuencias_t *f)
{
    return color_rapido_a_hsv(color_rapido_normalizar(cal, f));
}
