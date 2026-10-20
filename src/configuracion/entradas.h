/*
 * entradas.h — Parámetros de botones y joystick.
 */
#ifndef CONFIG_ENTRADAS_H
#define CONFIG_ENTRADAS_H

/* ---- Botones ----
   El módulo de B1..B4 es activo en ALTO (resistencia a GND y el botón lleva el pin a 3V3):
   comprobado en el banco, con 0 se leían los cuatro como pulsados. El botón del joystick
   es activo en bajo. Con 0 el pin usa pull-up; con 1, pull-down. */
#define BOTONES_ACTIVO_ALTO   1
#define JOY_SW_ACTIVO_ALTO    0
#define BOTON_ANTIREBOTE_US   20000u    /* nivel estable durante 20 ms para aceptarlo */
#define BOTON_LARGO_US       800000u    /* pulsación larga a los 800 ms               */

/* ---- Joystick analógico ---- */
#define JOY_CANAL_X           0u        /* AIN0 = GP26 */
#define JOY_CANAL_Y           1u        /* AIN1 = GP27 */
#define JOY_MUESTRAS_CAL      32u       /* lecturas promediadas para hallar el centro */
#define JOY_MUESTRAS          4u        /* lecturas promediadas por medición          */
#define JOY_ZONA_MUERTA       12        /* en la escala −100..100                     */
#define JOY_INVERTIR_X        0
#define JOY_INVERTIR_Y        1         /* arriba = positivo (igual que Lunar Lander) */

#endif /* CONFIG_ENTRADAS_H */
