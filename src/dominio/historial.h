/*
 * historial.h — Últimas lecturas de color guardadas, en un buffer circular.
 *
 * Vive en RAM: se pierde al reiniciar (guardarlo en flash queda para después).
 * Cuando está lleno, la lectura nueva sustituye a la más antigua.
 */
#ifndef HISTORIAL_H
#define HISTORIAL_H

#include <stdbool.h>
#include <stdint.h>
#include "dominio/color.h"

#define HISTORIAL_MAX  16

typedef struct {
    color_rgb_t rgb;
    int16_t     h;              /* tono 0..359, −1 si no está definido            */
    int32_t     x_um;           /* posición en la escala, −1 si no se situó       */
} entrada_hist_t;

typedef struct {
    entrada_hist_t entradas[HISTORIAL_MAX];
    uint8_t        cantidad;
    uint8_t        siguiente;   /* índice donde se escribirá la próxima */
} historial_t;

void historial_iniciar(historial_t *h);
void historial_agregar(historial_t *h, const entrada_hist_t *e);
uint8_t historial_cantidad(const historial_t *h);

/* Entrada número `indice` contando desde la MÁS RECIENTE (0 = la última). */
bool historial_obtener(const historial_t *h, uint8_t indice, entrada_hist_t *e);

#endif /* HISTORIAL_H */
