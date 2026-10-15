/*
 * texto.h — Dibujo de texto sobre la ST7789.
 *
 * Las cadenas son UTF-8 (se pueden escribir á, é, í, ó, ú, ñ, ü, ¿, ¡ y ° tal
 * cual en el código). Cada carácter se dibuja opaco, con color de fondo, y se
 * envía a la pantalla en una sola ventana: no hace falta borrar antes, solo
 * rellenar con espacios si el texto nuevo es más corto que el anterior.
 * No recorta: se deja de dibujar en el último carácter que cabe entero.
 */
#ifndef TEXTO_H
#define TEXTO_H

#include <stdint.h>

#define TEXTO_ESCALA_MAX  4

/* Tamaño de una celda de carácter (con separación) a la escala dada. */
#define TEXTO_CELDA_ANCHO(escala)  (6 * (escala))
#define TEXTO_CELDA_ALTO(escala)   (8 * (escala))

/* Número de caracteres (puntos de código) de una cadena UTF-8. */
int texto_longitud(const char *s);

/* Ancho en píxeles de la cadena a esa escala. */
int texto_ancho(const char *s, int escala);

/* Dibuja la cadena con la esquina superior izquierda en (x, y). */
void texto_dibujar(int x, int y, const char *s, int escala, uint16_t fg, uint16_t bg);

/* Igual, centrada en la franja horizontal [x, x + ancho). */
void texto_centrado(int x, int ancho, int y, const char *s, int escala, uint16_t fg, uint16_t bg);

/* Formatea con printf (máx. 63 bytes), rellena con espacios hasta `campo`
   caracteres (0 = sin relleno) y dibuja. Devuelve el ancho dibujado en píxeles. */
int texto_formato(int x, int y, int escala, uint16_t fg, uint16_t bg, int campo,
                  const char *formato, ...) __attribute__((format(printf, 7, 8)));

#endif /* TEXTO_H */
