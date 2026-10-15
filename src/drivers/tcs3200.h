/*
 * tcs3200.h — Lectura del sensor de color TCS3200 por registros.
 *
 * El TCS3200 convierte luz en una onda cuadrada cuya frecuencia es
 * proporcional a la intensidad que pasa por el filtro elegido con S2/S3.
 * La frecuencia se mide contando flancos de subida con una slice PWM en
 * modo DIVMODE = flanco (canal B), durante una ventana medida con el TIMER.
 *
 * La medición bloquea ≈ 4 × (espera de filtro + ventana) ≈ 90 ms: está
 * pensada para el estado MEASURE, con el motor detenido.
 */
#ifndef TCS3200_H
#define TCS3200_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TCS_FILTRO_ROJO,
    TCS_FILTRO_VERDE,
    TCS_FILTRO_AZUL,
    TCS_FILTRO_CLARO    /* sin filtro: intensidad total */
} tcs_filtro_t;

/* Frecuencias en Hz por canal. */
typedef struct {
    uint32_t r, g, b, c;
} tcs_frecuencias_t;

void tcs3200_iniciar(void);

/* Mide un solo canal. Devuelve la frecuencia en Hz; *desborde = true si el
   contador de 16 bits dio la vuelta (la lectura no es válida). */
uint32_t tcs3200_medir_canal(tcs_filtro_t filtro, bool *desborde);

/* Mide los cuatro canales. Devuelve false si alguno desbordó. */
bool tcs3200_medir(tcs_frecuencias_t *f);

#endif /* TCS3200_H */
