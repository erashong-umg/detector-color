/*
 * buzzer.h — Buzzer pasivo por PWM (slice 7, canal B) con reproducción sin bloqueo.
 *
 * Una nota fija TOP y el ciclo de trabajo de la slice; el contador corre sin
 * parar y la CPU solo interviene al cambiar de nota. buzzer_actualizar() debe
 * llamarse en cada vuelta del bucle principal para avanzar la secuencia.
 * En reposo la salida queda en BAJO (duty 0): un buzzer magnético con la
 * salida fija en alto consumiría corriente continua.
 */
#ifndef BUZZER_H
#define BUZZER_H

#include <stdbool.h>
#include <stdint.h>
#include "dominio/sonidos.h"

void buzzer_iniciar(void);

/* Empieza a sonar una secuencia; sustituye a la que estuviera sonando. */
void buzzer_reproducir(const sonido_t *sonido);

/* Una sola nota (hz = 0 → silencio) durante ms milisegundos. */
void buzzer_tono(uint16_t hz, uint16_t ms);

void buzzer_silenciar(void);
bool buzzer_ocupado(void);
void buzzer_actualizar(void);

#endif /* BUZZER_H */
