/*
 * botones.h — Botones con antirrebote por muestreo, sin bloqueos ni interrupciones.
 *
 * Hay que llamar a botones_actualizar() en cada vuelta del bucle principal
 * (cada ≤ 5 ms). Un cambio de nivel se acepta cuando se mantiene estable
 * BOTON_ANTIREBOTE_US. Los eventos (pulsado, soltado, largo) se consumen al
 * leerlos: cada pulsación se informa una sola vez.
 */
#ifndef BOTONES_H
#define BOTONES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BOTON_1,        /* iniciar detección */
    BOTON_2,        /* calibrar          */
    BOTON_3,        /* posicionar        */
    BOTON_4,        /* guardar / menú    */
    BOTON_JOY,      /* SW del joystick   */
    BOTON_CANTIDAD
} boton_id_t;

void botones_iniciar(void);
void botones_actualizar(void);

bool boton_presionado(boton_id_t b);    /* nivel: ¿está pulsado ahora? */
bool boton_pulsado(boton_id_t b);       /* evento: acaba de pulsarse   */
bool boton_soltado(boton_id_t b);       /* evento: acaba de soltarse   */
bool boton_largo(boton_id_t b);         /* evento: pulsación larga     */

/* Duración (ms) de la última pulsación terminada. */
uint32_t boton_ultima_duracion_ms(boton_id_t b);

#endif /* BOTONES_H */
