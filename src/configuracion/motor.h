/*
 * motor.h — Parámetros del motor DC (TB6612FNG), los finales de carrera IR
 * y los límites de seguridad del carrito.
 *
 * Todo lo marcado HIPÓTESIS hay que confirmarlo en el banco antes de
 * confiar en el sistema; el programa de prueba (pruebas/prueba_motor.c)
 * está pensado para eso.
 */
#ifndef CONFIG_MOTOR_H
#define CONFIG_MOTOR_H

/* ---- PWM del puente (slice 4, canal A) ----
   TOP = 125 MHz / 20 kHz − 1: fuera del rango audible. */
#define MOTOR_PWM_TOP           6249u

/* Un motor DC no arranca por debajo de cierto ciclo de trabajo: la
   velocidad 1..100 se reparte entre este mínimo y el 100 %.
   Medido en el banco con VM = 5 V: a 60 % se mueve 2-4 mm, a 80 % va a trompicones, a 90 % mejor y a 100 % va con
   fuerza. Si se sube VM o se cambia el motor, bajar este valor. */
#define MOTOR_DUTY_MIN_PCT      90u

/* Rampa: ms por cada punto de velocidad (100 puntos → 300 ms). */
#define MOTOR_RAMPA_MS_PASO     3u

/* Signo que convierte "hacia el FINAL" (+) en sentido eléctrico (IN1 alto = +).
   HIPÓTESIS: +1. Se verifica con el pulso de prueba; si el carrito va hacia
   el inicio con velocidad positiva, poner −1. */
#define MOTOR_SIGNO_HACIA_FIN   (+1)

/* ---- Finales de carrera IR ----
   HIPÓTESIS: salida digital activa en BAJO (nivel bajo = objeto delante),
   lo habitual en módulos con comparador LM393. Si es al revés, poner 0.
   El pull interno se elige hacia el estado ACTIVO: con un cable suelto el
   sensor se lee como "extremo alcanzado" y el carrito no se mueve (fallo seguro). */
#define FINAL_ACTIVO_BAJO       1
#define FINAL_ANTIREBOTE_MS     2u      /* nivel estable para aceptar el cambio (el carrito va rápido: 1 ms ≈ 0,4 mm) */

/* ---- Límites de seguridad ---- */
/* Tiempo máximo de movimiento continuo antes de parar con error.
   HIPÓTESIS: ajustar a ~1,5 × el tiempo real de recorrer los 350 mm. */
#define CARRO_TIMEOUT_MS        2500u

/* Velocidad máxima permitida en la prueba manual (% de 1..100). */
#define CARRO_VEL_MAX_PRUEBA    40

#endif /* CONFIG_MOTOR_H */
