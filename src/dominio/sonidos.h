/*
 * sonidos.h — Secuencias de notas del sistema (estilo control CNC) (datos puros, sin hardware).
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

extern const sonido_t SONIDO_INICIO;      /* "servo on": sistema listo, 3 notas ascendentes */
extern const sonido_t SONIDO_CLIC;        /* pulsación: tic agudo muy corto                  */
extern const sonido_t SONIDO_CONFIRMAR;   /* correcto: 2 pitidos iguales                     */
extern const sonido_t SONIDO_MOVIENDO;    /* el carrito arranca: rampa ascendente de eje     */
extern const sonido_t SONIDO_LLEGADA;     /* carrito "en posición": tic doble                */
extern const sonido_t SONIDO_HOME;        /* referencia terminada ("home done")              */
extern const sonido_t SONIDO_ANALIZANDO;  /* midiendo: barrido de sonda                      */
extern const sonido_t SONIDO_ALERTA;      /* aviso: 2 pitidos lentos                         */
extern const sonido_t SONIDO_ERROR;       /* alarma: sirena alterna agudo-grave              */

#endif /* SONIDOS_H */
