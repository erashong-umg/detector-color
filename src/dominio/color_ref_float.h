/*
 * color_ref_float.h — Versión de REFERENCIA con float (solo para medir en la Fase 3).
 *
 * El proyecto no la usa en producción: el Cortex-M0+ no tiene FPU, así que cada operación en
 * float es una llamada a una rutina de software (el SDK enlaza las de la ROM del RP2040).
 * Sirve de línea base para calcular el speedup de la versión entera y de la optimizada.
 */
#ifndef COLOR_REF_FLOAT_H
#define COLOR_REF_FLOAT_H

#include "dominio/color.h"

/* Misma cadena que color_normalizar() + color_a_hsv(), con float. Requiere calibración válida. */
color_hsv_t color_flotante_procesar(const color_calibracion_t *cal, const color_frecuencias_t *f);

#endif /* COLOR_REF_FLOAT_H */
