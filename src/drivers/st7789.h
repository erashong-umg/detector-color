/*
 * st7789.h — Driver de la pantalla ST7789 por registros del SPI0 (PL022).
 *
 * Dibuja directo en la GRAM del controlador (sin framebuffer en la Pico):
 * cada primitiva abre una ventana CASET/RASET y envía los píxeles RGB565.
 * Las coordenadas fuera de pantalla se recortan.
 */
#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>

/* Convierte RGB de 8 bits por canal a RGB565. */
#define ST7789_RGB565(r, g, b) \
    ((uint16_t)((((uint32_t)(r) & 0xF8u) << 8) | \
                (((uint32_t)(g) & 0xFCu) << 3) | \
                 ((uint32_t)(b) >> 3)))

#define ST7789_NEGRO    0x0000u
#define ST7789_BLANCO   0xFFFFu
#define ST7789_ROJO     0xF800u
#define ST7789_VERDE    0x07E0u
#define ST7789_AZUL     0x001Fu
#define ST7789_AMARILLO 0xFFE0u
#define ST7789_CIAN     0x07FFu
#define ST7789_MAGENTA  0xF81Fu

/* Configura SPI0 y pines, resetea el panel y lo deja en negro. */
void st7789_iniciar(void);

void st7789_rellenar(uint16_t color);
void st7789_rect(int x, int y, int ancho, int alto, uint16_t color);
void st7789_pixel(int x, int y, uint16_t color);

/* Copia un bloque de píxeles (fila a fila, ancho × alto) a la pantalla.
   No recorta: el bloque debe caber entero. */
void st7789_bloque(int x, int y, int ancho, int alto, const uint16_t *pixeles);

#endif /* ST7789_H */
