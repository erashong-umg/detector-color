#include "graficos/texto.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "configuracion/pantalla.h"
#include "drivers/st7789.h"
#include "graficos/fuente.h"

/* Celda de carácter a la escala máxima: 24 × 32 píxeles. */
static uint16_t celda[TEXTO_CELDA_ANCHO(TEXTO_ESCALA_MAX) * TEXTO_CELDA_ALTO(TEXTO_ESCALA_MAX)];

/* Decodifica el siguiente punto de código UTF-8 y avanza el puntero.
   Las secuencias que no se reconocen se devuelven como '?'. */
static uint32_t siguiente_codigo(const char **p)
{
    const uint8_t *s = (const uint8_t *)*p;
    uint32_t c = s[0];

    if (c == 0) {
        return 0;
    }
    if (c < 0x80) {
        *p += 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *p += 2;
        return ((c & 0x1Fu) << 6) | (s[1] & 0x3Fu);
    }
    /* Secuencia de 3 o 4 bytes (símbolos que esta fuente no tiene): se salta entera. */
    int largo = (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
    for (int i = 1; i < largo && (s[i] & 0xC0) == 0x80; i++) {
        *p += 1;
    }
    *p += 1;
    return '?';
}

int texto_longitud(const char *s)
{
    int n = 0;
    while (siguiente_codigo(&s) != 0) {
        n++;
    }
    return n;
}

int texto_ancho(const char *s, int escala)
{
    return texto_longitud(s) * TEXTO_CELDA_ANCHO(escala);
}

static void dibujar_celda(int x, int y, uint32_t codigo, int escala, uint16_t fg, uint16_t bg)
{
    const uint8_t *g = fuente_glifo(codigo);
    const int ancho = TEXTO_CELDA_ANCHO(escala);
    const int alto = TEXTO_CELDA_ALTO(escala);

    for (int py = 0; py < alto; py++) {
        int fila = py / escala;                     /* 0..7; la fila 7 es separación */
        for (int px = 0; px < ancho; px++) {
            int col = px / escala;                  /* 0..5; la columna 5 es separación */
            bool encendido = fila < FUENTE_FILAS && col < FUENTE_COLUMNAS &&
                             (g[col] & (1u << fila));
            celda[py * ancho + px] = encendido ? fg : bg;
        }
    }
    st7789_bloque(x, y, ancho, alto, celda);
}

void texto_dibujar(int x, int y, const char *s, int escala, uint16_t fg, uint16_t bg)
{
    if (escala < 1 || escala > TEXTO_ESCALA_MAX || y < 0 ||
        y + TEXTO_CELDA_ALTO(escala) > TFT_ALTO) {
        return;
    }
    for (uint32_t c = siguiente_codigo(&s); c != 0; c = siguiente_codigo(&s)) {
        if (x >= 0 && x + TEXTO_CELDA_ANCHO(escala) <= TFT_ANCHO) {
            dibujar_celda(x, y, c, escala, fg, bg);
        }
        x += TEXTO_CELDA_ANCHO(escala);
        if (x >= TFT_ANCHO) {
            break;
        }
    }
}

void texto_centrado(int x, int ancho, int y, const char *s, int escala, uint16_t fg, uint16_t bg)
{
    int w = texto_ancho(s, escala);
    texto_dibujar(x + (ancho - w) / 2, y, s, escala, fg, bg);
}

int texto_formato(int x, int y, int escala, uint16_t fg, uint16_t bg, int campo,
                  const char *formato, ...)
{
    char buf[64];
    va_list ap;

    va_start(ap, formato);
    vsnprintf(buf, sizeof buf, formato, ap);
    va_end(ap);

    /* Relleno con espacios hasta `campo` caracteres (no hay otro modo de borrar el texto previo). */
    int largo = texto_longitud(buf);
    size_t bytes = strlen(buf);
    while (largo < campo && bytes + 1 < sizeof buf) {
        buf[bytes++] = ' ';
        largo++;
    }
    buf[bytes] = '\0';

    texto_dibujar(x, y, buf, escala, fg, bg);
    return largo * TEXTO_CELDA_ANCHO(escala);
}
