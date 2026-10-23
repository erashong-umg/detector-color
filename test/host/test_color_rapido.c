/*
 * test_color_rapido.c — Comprueba en el PC la versión optimizada (color_rapido) y la de referencia
 * con float contra color.c.
 *
 *   1. HSV: recorre los 16 777 216 colores RGB; color_rapido_a_hsv debe ser IDÉNTICO a color_a_hsv.
 *   2. Normalización: muestras aleatorias con varias calibraciones; la diferencia máxima con
 *      color_normalizar debe ser ≤ 1.
 *   3. Cadena completa: reporta la diferencia máxima de la versión float respecto a la entera.
 *
 * Compilar (desde la raíz del repositorio):
 *   gcc -O2 -Wall -Wextra -Isrc test/host/test_color_rapido.c src/dominio/color.c \
 *       src/dominio/color_rapido.c src/dominio/color_ref_float.c -o test/host/build/test_color_rapido.exe
 * Devuelve distinto de cero si algo falla.
 */
#include <stdio.h>
#include <stdlib.h>

#include "dominio/color.h"
#include "dominio/color_rapido.h"
#include "dominio/color_ref_float.h"

static uint32_t semilla = 2463534242u;
static uint32_t aleatorio(void)
{
    semilla ^= semilla << 13;
    semilla ^= semilla >> 17;
    semilla ^= semilla << 5;
    return semilla;
}

static int abs_i(int x) { return x < 0 ? -x : x; }
static int dif_h(int a, int b)
{
    if (a < 0 || b < 0) return a == b ? 0 : 999;
    int d = abs_i(a - b);
    return d > 180 ? 360 - d : d;
}

int main(void)
{
    int fallos = 0;
    color_rapido_iniciar();

    /* 1. HSV exhaustivo */
    uint32_t distintos = 0;
    for (uint32_t r = 0; r < 256; r++)
        for (uint32_t g = 0; g < 256; g++)
            for (uint32_t b = 0; b < 256; b++) {
                color_rgb_t rgb = { (uint8_t)r, (uint8_t)g, (uint8_t)b };
                color_hsv_t a = color_a_hsv(rgb), c = color_rapido_a_hsv(rgb);
                if (a.h != c.h || a.s != c.s || a.v != c.v) {
                    if (distintos < 5)
                        printf("  distinto en rgb(%u,%u,%u): h %d/%d s %d/%d v %d/%d\n", r, g, b,
                               a.h, c.h, a.s, c.s, a.v, c.v);
                    distintos++;
                }
            }
    printf("1. HSV exhaustivo (16777216 colores): %u diferencias -> %s\n",
           distintos, distintos == 0 ? "OK" : "FALLA");
    fallos += distintos != 0;

    /* 2 y 3. Normalización y cadena completa con calibraciones aleatorias */
    int max_norm = 0, max_dh = 0, max_ds = 0, max_dv = 0;
    uint32_t n_float_dist = 0, total = 0;
    for (int k = 0; k < 2000; k++) {
        color_calibracion_t cal = { 0 };
        cal.hay_negro = cal.hay_blanco = true;
        cal.negro.r  = 1000 + aleatorio() % 6000;  cal.blanco.r = 20000 + aleatorio() % 150000;
        cal.negro.g  = 1000 + aleatorio() % 6000;  cal.blanco.g = 20000 + aleatorio() % 150000;
        cal.negro.b  = 1000 + aleatorio() % 6000;  cal.blanco.b = 20000 + aleatorio() % 150000;
        color_rapido_cal_t rap;
        if (!color_rapido_preparar(&cal, &rap)) { printf("preparar falló\n"); return 1; }

        for (int i = 0; i < 500; i++) {
            color_frecuencias_t f;
            f.r = aleatorio() % (cal.blanco.r + 4000);
            f.g = aleatorio() % (cal.blanco.g + 4000);
            f.b = aleatorio() % (cal.blanco.b + 4000);
            f.c = 0;

            color_rgb_t rb;
            color_normalizar(&cal, &f, &rb);
            color_rgb_t rc = color_rapido_normalizar(&rap, &f);
            int d = abs_i(rb.r - rc.r);
            if (abs_i(rb.g - rc.g) > d) d = abs_i(rb.g - rc.g);
            if (abs_i(rb.b - rc.b) > d) d = abs_i(rb.b - rc.b);
            if (d > max_norm) max_norm = d;

            color_hsv_t hb = color_a_hsv(rb), hf = color_flotante_procesar(&cal, &f);
            if (dif_h(hb.h, hf.h) > max_dh) max_dh = dif_h(hb.h, hf.h);
            if (abs_i(hb.s - hf.s) > max_ds) max_ds = abs_i(hb.s - hf.s);
            if (abs_i(hb.v - hf.v) > max_dv) max_dv = abs_i(hb.v - hf.v);
            if (hb.h != hf.h || hb.s != hf.s || hb.v != hf.v) n_float_dist++;
            total++;
        }
    }
    printf("2. Normalización rápida vs color.c: diferencia máxima = %d -> %s\n",
           max_norm, max_norm <= 1 ? "OK" : "FALLA");
    fallos += max_norm > 1;
    printf("3. Versión float vs entera (%u muestras): dif. máx. h=%d s=%d v=%d, %u muestras con alguna diferencia\n",
           total, max_dh, max_ds, max_dv, n_float_dist);

    printf(fallos ? "RESULTADO: %d fallo(s)\n" : "RESULTADO: todo correcto\n", fallos);
    return fallos != 0;
}
