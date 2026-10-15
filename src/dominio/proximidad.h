/*
 * proximidad.h — Clasificación de la distancia de la muestra, sin hardware.
 *
 * Trabaja con un "nivel" 0..4095 donde mayor = más cerca (el driver invierte
 * la lectura si el módulo funciona al revés) y con la salida digital de
 * presencia. Aplica histéresis en cada umbral y un filtro de muestras
 * consecutivas, de modo que el estado no parpadee.
 */
#ifndef PROXIMIDAD_H
#define PROXIMIDAD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PROX_SIN_OBJETO = 0,
    PROX_LEJOS,             /* hay objeto pero hay que acercarlo   */
    PROX_OK,                /* distancia correcta                  */
    PROX_CERCA              /* demasiado cerca                     */
} prox_estado_t;

typedef struct {
    prox_estado_t estado;       /* estado estable */
    prox_estado_t candidato;
    uint8_t       cuenta;
} prox_filtro_t;

void prox_filtro_iniciar(prox_filtro_t *f);

/* Clasificación con histéresis a partir del estado actual. Con la salida
   digital activa, el estado es como mínimo PROX_LEJOS. */
prox_estado_t prox_clasificar(uint16_t nivel, bool digital_activo, prox_estado_t actual);

/* Incorpora una lectura; el estado estable cambia tras N lecturas iguales. */
void prox_filtro_actualizar(prox_filtro_t *f, uint16_t nivel, bool digital_activo);

/* Texto corto para la pantalla (≤ 5 caracteres). */
const char *prox_texto(prox_estado_t e);

#endif /* PROXIMIDAD_H */
