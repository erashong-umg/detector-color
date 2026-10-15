/*
 * prueba_pantalla.c — Etapa 2 del plan: prueba aislada de la ST7789.
 *
 * Qué verificar en la pantalla:
 *   1. Rojo, verde, azul, blanco y negro a pantalla completa, en ese orden
 *      (si salen cambiados o invertidos → configuracion/pantalla.h).
 *   2. Patrón fijo:
 *      - marco blanco de 1 px pegado a los cuatro bordes → geometría y offsets;
 *      - barras: blanco, amarillo, cian, verde, magenta, rojo, azul, negro;
 *      - escala de tono 0°→360° (rojo→amarillo→verde→cian→azul→magenta→rojo),
 *        la misma idea que la escala impresa de 350 mm;
 *      - rampa de grises de negro a blanco.
 * Por USB se imprime el tiempo de cada relleno completo; el teórico a
 * 62,5 MHz es 240·240·16 / 62,5 MHz ≈ 14,7 ms.
 */
#include <stdbool.h>
#include <stdio.h>

#include "pruebas/pruebas.h"
#include "configuracion/pantalla.h"
#include "drivers/st7789.h"
#include "hw/tiempo.h"

/* Tono en 0..1535 (6 sectores de 256) → RGB565 con saturación y brillo máximos. */
static uint16_t tono_a_rgb565(uint32_t tono)
{
    uint32_t f = tono & 0xFFu;
    uint32_t r = 0, g = 0, b = 0;

    switch (tono >> 8) {
    case 0:  r = 255;     g = f;       b = 0;       break; /* rojo → amarillo    */
    case 1:  r = 255 - f; g = 255;     b = 0;       break; /* amarillo → verde   */
    case 2:  r = 0;       g = 255;     b = f;       break; /* verde → cian       */
    case 3:  r = 0;       g = 255 - f; b = 255;     break; /* cian → azul        */
    case 4:  r = f;       g = 0;       b = 255;     break; /* azul → magenta     */
    default: r = 255;     g = 0;       b = 255 - f; break; /* magenta → rojo     */
    }
    return ST7789_RGB565(r, g, b);
}

static void prueba_rellenos(void)
{
    static const struct { uint16_t color; const char *nombre; } rellenos[] = {
        { ST7789_ROJO,   "rojo"   },
        { ST7789_VERDE,  "verde"  },
        { ST7789_AZUL,   "azul"   },
        { ST7789_BLANCO, "blanco" },
        { ST7789_NEGRO,  "negro"  },
    };

    for (unsigned i = 0; i < sizeof rellenos / sizeof rellenos[0]; i++) {
        uint32_t t0 = tiempo_us();
        st7789_rellenar(rellenos[i].color);
        uint32_t dt = tiempo_us() - t0;
        printf("relleno %-6s %lu us\n", rellenos[i].nombre, (unsigned long)dt);
        tiempo_esperar_ms(500);
    }
}

static void dibujar_patron(void)
{
    static const uint16_t barras[8] = {
        ST7789_BLANCO, ST7789_AMARILLO, ST7789_CIAN,  ST7789_VERDE,
        ST7789_MAGENTA, ST7789_ROJO,    ST7789_AZUL,  ST7789_NEGRO
    };
    const int ancho_barra = TFT_ANCHO / 8;

    st7789_rellenar(ST7789_NEGRO);

    /* Barras de color: filas 10..69 */
    for (int i = 0; i < 8; i++) {
        st7789_rect(i * ancho_barra, 10, ancho_barra, 60, barras[i]);
    }

    /* Escala de tono: filas 80..159, una columna por paso */
    static uint16_t columna[80];
    for (int x = 0; x < TFT_ANCHO; x++) {
        uint16_t c = tono_a_rgb565((uint32_t)x * 1536u / TFT_ANCHO);
        for (int y = 0; y < 80; y++) {
            columna[y] = c;
        }
        st7789_bloque(x, 80, 1, 80, columna);
    }

    /* Rampa de grises: filas 170..229 */
    for (int x = 0; x < TFT_ANCHO; x++) {
        uint32_t v = (uint32_t)x * 255u / (TFT_ANCHO - 1);
        st7789_rect(x, 170, 1, 60, ST7789_RGB565(v, v, v));
    }

    /* Marco de 1 px en el borde exacto */
    st7789_rect(0, 0, TFT_ANCHO, 1, ST7789_BLANCO);
    st7789_rect(0, TFT_ALTO - 1, TFT_ANCHO, 1, ST7789_BLANCO);
    st7789_rect(0, 0, 1, TFT_ALTO, ST7789_BLANCO);
    st7789_rect(TFT_ANCHO - 1, 0, 1, TFT_ALTO, ST7789_BLANCO);
}

void prueba_pantalla(void)
{
    st7789_iniciar();
    printf("ST7789 iniciada\n");

    prueba_rellenos();
    dibujar_patron();
    printf("Patron dibujado. Revisar marco, barras, escala de tono y grises.\n");

    while (true) {
        tiempo_esperar_ms(1000);
    }
}
