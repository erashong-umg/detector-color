/*
 * fuente.h — Fuente de mapa de bits 5×7 (datos generados por tools/generar_fuente.py).
 */
#ifndef FUENTE_H
#define FUENTE_H

#include <stdint.h>

#define FUENTE_COLUMNAS     5
#define FUENTE_FILAS        7
#define FUENTE_CELDA_ANCHO  (FUENTE_COLUMNAS + 1)   /* glifo + 1 columna de separación */
#define FUENTE_CELDA_ALTO   (FUENTE_FILAS + 1)      /* glifo + 1 fila de separación    */

/* Devuelve las 5 columnas del glifo del punto de código Unicode dado (bit 0 =
   fila superior). Para un carácter que la fuente no tiene devuelve el de '?'. */
const uint8_t *fuente_glifo(uint32_t codigo);

#endif /* FUENTE_H */
