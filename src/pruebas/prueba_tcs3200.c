/*
 * prueba_tcs3200.c — Etapa 3 del plan: leer y calibrar el TCS3200.
 *
 * Mide los 4 canales en bucle y lo imprime por USB. Comandos por el
 * terminal serie (una tecla, sin Enter):
 *   n  calibrar NEGRO  (muestra negra mate frente al sensor)
 *   b  calibrar BLANCO (papel blanco a la misma distancia)
 *   c  borrar la calibración
 *
 * En la ST7789 (si está conectada):
 *   - arriba, la muestra del color medido (gris oscuro si no hay calibración);
 *   - abajo, barras R, G y B de 0 a 255;
 *   - cuadrito en la esquina: verde = calibrado, rojo = sin calibrar,
 *     amarillo = el contador desbordó (lectura no válida).
 *
 * Qué comprobar en el banco:
 *   - Con la muestra fija, las frecuencias varían poco entre lecturas (< 1–2 %).
 *   - Tapar el sensor baja las 4 frecuencias; más luz las sube.
 *   - Tras calibrar: blanco ≈ #FFFFFF, negro ≈ #000000, y una muestra roja,
 *     verde o azul da el canal dominante correcto.
 *   - Con el IR de proximidad conectado/desconectado, la misma muestra no
 *     debe cambiar (ver docs/hardware.md §4).
 */
#include <stdio.h>
#include "pico/stdlib.h"            /* getchar_timeout_us(): stdio por USB */

#include "pruebas/pruebas.h"
#include "drivers/st7789.h"
#include "drivers/tcs3200.h"
#include "dominio/color.h"
#include "configuracion/pantalla.h"
#include "hw/tiempo.h"

#define GRIS_OSCURO  ST7789_RGB565(40, 40, 40)

static color_frecuencias_t a_dominio(const tcs_frecuencias_t *f)
{
    color_frecuencias_t c = { f->r, f->g, f->b, f->c };
    return c;
}

static void atender_comando(color_calibracion_t *cal, const color_frecuencias_t *f)
{
    int tecla = getchar_timeout_us(0);

    switch (tecla) {
    case 'n': case 'N':
        cal->negro = *f;
        cal->hay_negro = true;
        printf(">> NEGRO calibrado\n");
        break;
    case 'b': case 'B':
        cal->blanco = *f;
        cal->hay_blanco = true;
        printf(">> BLANCO calibrado\n");
        break;
    case 'c': case 'C':
        cal->hay_negro = cal->hay_blanco = false;
        printf(">> calibracion borrada\n");
        break;
    default:
        break;
    }
}

static void dibujar_barra(int y, uint8_t valor, uint16_t color)
{
    int largo = valor * TFT_ANCHO / 255;
    st7789_rect(0, y, largo, 20, color);
    st7789_rect(largo, y, TFT_ANCHO - largo, 20, ST7789_NEGRO);
}

static void dibujar(bool hay_rgb, color_rgb_t rgb, bool valida, bool calibrado)
{
    st7789_rect(0, 0, TFT_ANCHO, 140,
                hay_rgb ? ST7789_RGB565(rgb.r, rgb.g, rgb.b) : GRIS_OSCURO);

    uint16_t indicador = !valida ? ST7789_AMARILLO
                       : calibrado ? ST7789_VERDE : ST7789_ROJO;
    st7789_rect(TFT_ANCHO - 20, 4, 16, 16, indicador);

    dibujar_barra(152, hay_rgb ? rgb.r : 0, ST7789_ROJO);
    dibujar_barra(182, hay_rgb ? rgb.g : 0, ST7789_VERDE);
    dibujar_barra(212, hay_rgb ? rgb.b : 0, ST7789_AZUL);
}

void prueba_tcs3200(void)
{
    color_calibracion_t cal = { 0 };

    st7789_iniciar();
    tcs3200_iniciar();
    printf("TCS3200 listo. Teclas: n = negro, b = blanco, c = borrar calibracion\n");

    while (true) {
        tcs_frecuencias_t medida;
        uint32_t t0 = tiempo_us();
        bool valida = tcs3200_medir(&medida);
        uint32_t dt_ms = (tiempo_us() - t0) / 1000u;

        color_frecuencias_t f = a_dominio(&medida);
        atender_comando(&cal, &f);

        printf("Hz R=%6lu G=%6lu B=%6lu C=%6lu (%lu ms)%s",
               (unsigned long)f.r, (unsigned long)f.g,
               (unsigned long)f.b, (unsigned long)f.c,
               (unsigned long)dt_ms, valida ? "" : " DESBORDE");

        color_rgb_t rgb;
        bool hay_rgb = valida && color_normalizar(&cal, &f, &rgb);
        if (hay_rgb) {
            char hex[8];
            color_a_hex(rgb, hex);
            color_hsv_t hsv = color_a_hsv(rgb);
            printf(" | RGB=%3u,%3u,%3u %s H=%d S=%u V=%u\n",
                   rgb.r, rgb.g, rgb.b, hex, hsv.h, hsv.s, hsv.v);
        } else if (color_calibrado(&cal)) {
            printf(" | calibracion invalida (blanco <= negro en algun canal)\n");
        } else {
            printf(" | sin calibrar%s%s\n",
                   cal.hay_negro ? "" : " [falta n]",
                   cal.hay_blanco ? "" : " [falta b]");
        }

        dibujar(hay_rgb, rgb, valida, color_calibrado(&cal));
        tiempo_esperar_ms(200);
    }
}
