/*
 * color.c — Normalización, HEX y HSV con aritmética entera (el M0+ no tiene FPU).
 */
#include "dominio/color.h"

static bool normalizar_canal(uint32_t f, uint32_t negro, uint32_t blanco, uint8_t *v)
{
    if (blanco <= negro) {
        return false;
    }
    if (f <= negro) {
        *v = 0;
    } else if (f >= blanco) {
        *v = 255;
    } else {
        *v = (uint8_t)(((uint64_t)(f - negro) * 255u + (blanco - negro) / 2u) / (blanco - negro));
    }
    return true;
}

bool color_normalizar(const color_calibracion_t *cal,
                      const color_frecuencias_t *f, color_rgb_t *rgb)
{
    if (!color_calibrado(cal)) {
        return false;
    }
    return normalizar_canal(f->r, cal->negro.r, cal->blanco.r, &rgb->r)
        && normalizar_canal(f->g, cal->negro.g, cal->blanco.g, &rgb->g)
        && normalizar_canal(f->b, cal->negro.b, cal->blanco.b, &rgb->b);
}

void color_a_hex(color_rgb_t rgb, char hex[8])
{
    static const char digitos[] = "0123456789ABCDEF";
    const uint8_t v[3] = { rgb.r, rgb.g, rgb.b };

    hex[0] = '#';
    for (int i = 0; i < 3; i++) {
        hex[1 + 2 * i] = digitos[v[i] >> 4];
        hex[2 + 2 * i] = digitos[v[i] & 0xF];
    }
    hex[7] = '\0';
}

color_hsv_t color_a_hsv(color_rgb_t rgb)
{
    int r = rgb.r, g = rgb.g, b = rgb.b;
    int max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    int min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    int delta = max - min;
    color_hsv_t hsv;

    hsv.v = (uint8_t)max;
    hsv.s = max == 0 ? 0 : (uint8_t)(delta * 255 / max);

    if (delta == 0) {
        hsv.h = -1;
        return hsv;
    }

    int h;
    if (max == r) {
        h = 60 * (g - b) / delta;           /* −60..60   */
    } else if (max == g) {
        h = 120 + 60 * (b - r) / delta;     /* 60..180   */
    } else {
        h = 240 + 60 * (r - g) / delta;     /* 180..300  */
    }
    if (h < 0) {
        h += 360;
    }
    hsv.h = (int16_t)(h % 360);
    return hsv;
}

color_rgb_t color_desde_hsv(color_hsv_t hsv)
{
    color_rgb_t rgb;
    int v = hsv.v, s = hsv.s;

    if (s == 0 || hsv.h < 0) {
        rgb.r = rgb.g = rgb.b = (uint8_t)v;
        return rgb;
    }

    int h = hsv.h % 360;
    int sector = h / 60;
    int resto = (h % 60) * 255 / 60;                 /* 0..250 */
    int p = v * (255 - s) / 255;
    int q = v * (255 - s * resto / 255) / 255;
    int t = v * (255 - s * (255 - resto) / 255) / 255;

    switch (sector) {
    case 0:  rgb.r = v; rgb.g = t; rgb.b = p; break;
    case 1:  rgb.r = q; rgb.g = v; rgb.b = p; break;
    case 2:  rgb.r = p; rgb.g = v; rgb.b = t; break;
    case 3:  rgb.r = p; rgb.g = q; rgb.b = v; break;
    case 4:  rgb.r = t; rgb.g = p; rgb.b = v; break;
    default: rgb.r = v; rgb.g = p; rgb.b = q; break;
    }
    return rgb;
}
