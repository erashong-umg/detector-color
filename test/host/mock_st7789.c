/* st7789 simulado: escribe en un framebuffer RGB565 de 240x320 y lo vuelca a PPM. */
#include "drivers/st7789.h"
#include "configuracion/pantalla.h"
#include <stdio.h>
#include <string.h>

uint16_t fb[TFT_ALTO][TFT_ANCHO];
int fuera_de_pantalla;      /* intentos de dibujar fuera de los límites */
long pixeles_escritos;

void st7789_iniciar(void) { memset(fb, 0, sizeof fb); }

void st7789_rect(int x, int y, int ancho, int alto, uint16_t color)
{
    if (x < 0) { ancho += x; x = 0; }
    if (y < 0) { alto += y; y = 0; }
    if (x + ancho > TFT_ANCHO) ancho = TFT_ANCHO - x;
    if (y + alto > TFT_ALTO) alto = TFT_ALTO - y;
    if (ancho <= 0 || alto <= 0) return;
    for (int j = 0; j < alto; j++)
        for (int i = 0; i < ancho; i++) fb[y + j][x + i] = color;
    pixeles_escritos += (long)ancho * alto;
}

void st7789_rellenar(uint16_t color) { st7789_rect(0, 0, TFT_ANCHO, TFT_ALTO, color); }
void st7789_pixel(int x, int y, uint16_t color) { st7789_rect(x, y, 1, 1, color); }

void st7789_bloque(int x, int y, int ancho, int alto, const uint16_t *p)
{
    if (x < 0 || y < 0 || ancho <= 0 || alto <= 0 || x + ancho > TFT_ANCHO || y + alto > TFT_ALTO) {
        fuera_de_pantalla++;       /* el driver real descarta el bloque entero */
        return;
    }
    for (int j = 0; j < alto; j++)
        for (int i = 0; i < ancho; i++) fb[y + j][x + i] = p[j * ancho + i];
    pixeles_escritos += (long)ancho * alto;
}

void volcar_ppm(const char *ruta)
{
    FILE *f = fopen(ruta, "wb");
    fprintf(f, "P6\n%d %d\n255\n", TFT_ANCHO, TFT_ALTO);
    for (int y = 0; y < TFT_ALTO; y++)
        for (int x = 0; x < TFT_ANCHO; x++) {
            uint16_t c = fb[y][x];
            unsigned char px[3] = { (unsigned char)(((c >> 11) & 0x1F) * 255 / 31),
                                    (unsigned char)(((c >> 5) & 0x3F) * 255 / 63),
                                    (unsigned char)((c & 0x1F) * 255 / 31) };
            fwrite(px, 1, 3, f);
        }
    fclose(f);
}
