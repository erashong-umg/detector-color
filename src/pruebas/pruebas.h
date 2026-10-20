/*
 * pruebas.h — Programas de prueba por etapa del plan de desarrollo.
 * main.c elige cuál ejecutar con PRUEBA_ACTIVA. Ninguna retorna.
 */
#ifndef PRUEBAS_H
#define PRUEBAS_H

void prueba_pantalla(void);     /* etapa 2: ST7789                 */
void prueba_tcs3200(void);      /* etapa 3: lectura y calibración  */
void prueba_entradas(void);     /* etapa 4: botones y joystick     */
void prueba_actuadores(void);   /* etapa 4b: buzzer                 */
void prueba_motor(void);        /* etapa 5: TB6612, motor, finales */
void prueba_eje(void);          /* etapa 6: homing y posición      */
void prueba_mapeo(void);        /* etapa 7: color → posición       */
void prueba_proximidad(void);   /* etapa 9: diagnóstico del IR     */

#endif /* PRUEBAS_H */
