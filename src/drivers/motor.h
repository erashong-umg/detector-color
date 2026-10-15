/*
 * motor.h — Motor DC con el puente TB6612FNG (canal A), por registros.
 *
 * Tabla de verdad del TB6612FNG (IN1 IN2 PWM STBY):
 *   H  L  H  H   giro +        L  H  H  H   giro −
 *   H  H  x  H   freno (cortocircuito del motor)
 *   L  L  H  H   parada libre  x  x  x  L   standby (puente apagado, libre)
 *
 * Arranca SIEMPRE en standby (STBY = 0, salidas a 0). El motor no se mueve
 * hasta motor_habilitar(true) y una velocidad distinta de cero.
 *
 * La velocidad (−100..100) llega con rampa: motor_actualizar() debe llamarse
 * en cada vuelta del bucle principal. Los cambios de sentido pasan por cero.
 * motor_frenar() es inmediato y no usa rampa (parada de emergencia).
 */
#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>

void motor_iniciar(void);

void motor_habilitar(bool habilitar);
bool motor_habilitado(void);

/* Velocidad objetivo; la real la alcanza con rampa. 0 = parada libre con rampa. */
void motor_velocidad(int velocidad);

/* Freno inmediato por cortocircuito; deja objetivo y velocidad real en 0. */
void motor_frenar(void);

void motor_actualizar(void);

int motor_velocidad_real(void);

#endif /* MOTOR_H */
