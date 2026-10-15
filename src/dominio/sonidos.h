/*
 * sonidos.h — Secuencias de notas del sistema (datos puros, sin hardware).
 * hz = 0 es una pausa.
 */
#ifndef SONIDOS_H
#define SONIDOS_H

#include <stdint.h>

typedef struct {
    uint16_t hz;
    uint16_t ms;
} nota_t;

typedef struct {
    const nota_t *notas;
    uint8_t       cantidad;
} sonido_t;

extern const sonido_t SONIDO_INICIO;      /* arranque terminado            */
extern const sonido_t SONIDO_CLIC;        /* pulsación de botón            */
extern const sonido_t SONIDO_CONFIRMAR;   /* medición / guardado correcto  */
extern const sonido_t SONIDO_ALERTA;      /* "acerque la muestra", aviso   */
extern const sonido_t SONIDO_ERROR;       /* fallo, parada segura          */

#endif /* SONIDOS_H */
