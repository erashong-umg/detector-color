/*
 * entradas.h — Parámetros de botones y joystick.
 */
#ifndef CONFIG_ENTRADAS_H
#define CONFIG_ENTRADAS_H

/* ---- Botones (activos en bajo, con pull-up) ---- */
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
