/*
 * main.c — Detector de color con cursor cromático lineal.
 *
 * Mientras se valida el hardware por etapas, main solo arranca el sistema
 * y lanza el programa de prueba elegido. Cambiar PRUEBA_ACTIVA y recompilar.
 */
#include <stdio.h>
#include "pico/stdlib.h"            /* solo stdio_init_all(): printf por USB */

#include "aplicacion/app.h"
#include "pruebas/pruebas.h"
#include "hw/pin.h"
#include "hw/tiempo.h"

#define PRUEBA_PANTALLA     1
#define PRUEBA_TCS3200      2
#define PRUEBA_ENTRADAS     3
#define PRUEBA_ACTUADORES   4
#define PRUEBA_MOTOR        5
#define PRUEBA_EJE          6
#define PRUEBA_MAPEO        7
#define PRUEBA_PROXIMIDAD   8
#define MODO_APLICACION     9

#define PRUEBA_ACTIVA       MODO_APLICACION

int main(void)
{
    stdio_init_all();
    tiempo_esperar_ms(2000);    /* da tiempo a que Windows enumere el puerto USB */

    printf("\nDetector de color - firmware por registros (RP2040)\n");
    pin_inicializar();

#if PRUEBA_ACTIVA == PRUEBA_PANTALLA
    prueba_pantalla();
#elif PRUEBA_ACTIVA == PRUEBA_TCS3200
    prueba_tcs3200();
#elif PRUEBA_ACTIVA == PRUEBA_ENTRADAS
    prueba_entradas();
#elif PRUEBA_ACTIVA == PRUEBA_ACTUADORES
    prueba_actuadores();
#elif PRUEBA_ACTIVA == PRUEBA_MOTOR
    prueba_motor();
#elif PRUEBA_ACTIVA == PRUEBA_EJE
    prueba_eje();
#elif PRUEBA_ACTIVA == PRUEBA_MAPEO
    prueba_mapeo();
#elif PRUEBA_ACTIVA == PRUEBA_PROXIMIDAD
    prueba_proximidad();
#elif PRUEBA_ACTIVA == MODO_APLICACION
    app_ejecutar();
#else
#error "PRUEBA_ACTIVA no válida"
#endif
}
