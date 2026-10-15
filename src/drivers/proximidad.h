/*
 * proximidad.h — IR de proximidad (OA → ADC2 en GP28, OD → GP1).
 *
 * Hace falta adc_iniciar() antes. proximidad_actualizar() debe llamarse en
 * cada vuelta del bucle: muestrea cada PROX_PERIODO_MS y aplica el filtro.
 */
#ifndef PROXIMIDAD_DRIVER_H
#define PROXIMIDAD_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "dominio/proximidad.h"

void proximidad_iniciar(void);
void proximidad_actualizar(void);

/* Estado filtrado. Con PROX_HABILITADO = 0 devuelve siempre PROX_OK. */
prox_estado_t proximidad_estado(void);

/* Para diagnóstico: lectura cruda del ADC (0..4095, sin invertir) y de OD (nivel eléctrico). */
uint16_t proximidad_adc_crudo(void);
bool     proximidad_od_nivel(void);

/* Nivel ya orientado (mayor = más cerca) y salida digital interpretada. */
uint16_t proximidad_nivel(void);
bool     proximidad_digital_activo(void);

#endif /* PROXIMIDAD_DRIVER_H */
