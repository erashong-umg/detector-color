/*
 * interfaz.h — Interfaz gráfica de la ST7789 (240×240): pantalla principal,
 * menú, historial, mover cursor e información.
 *
 * No toca el sensor, el motor ni los botones: recibe una copia del estado del
 * sistema (ui_modelo_t) y las entradas ya interpretadas (ui_entrada_t) y
 * devuelve una orden (ui_orden_t) que la aplicación ejecuta. Así se prueba y
 * se reutiliza igual con cualquier orquestador.
 *
 * ui_dibujar() es COOPERATIVA: pinta una sola sección por llamada (≤ ≈ 5 ms),
 * de modo que el bucle principal no deja de vigilar el motor y los finales de
 * carrera. Llamarla en cada vuelta; refresca toda la pantalla en ≈ 6 llamadas.
 * Cambiar de pantalla sí borra todo de una vez (≈ 15 ms).
 */
#ifndef INTERFAZ_H
#define INTERFAZ_H

#include <stdbool.h>
#include <stdint.h>
#include "dominio/color.h"
#include "dominio/escala.h"
#include "dominio/historial.h"

typedef enum {
    UI_PANTALLA_PRINCIPAL,
    UI_PANTALLA_MENU,
    UI_PANTALLA_HISTORIAL,
    UI_PANTALLA_MOVER,
    UI_PANTALLA_INFO
} ui_pantalla_t;

/* Estado del sistema que la interfaz muestra. Las cadenas se copian al
   modelo para que no dependan de punteros que cambien. */
typedef struct {
    char     estado[12];            /* "LISTO", "MIDIENDO", "MOVIENDO"...         */
    uint16_t estado_color;          /* RGB565                                     */
    bool     chip_wifi, chip_cal, chip_ref, chip_arm;

    bool        hay_lectura;
    color_rgb_t rgb;
    char        hex[8];
    int16_t     h;                  /* tono 0..359, −1 sin definir                */
    uint8_t     s, v;               /* 0..255                                     */
    escala_clase_t clase;

    int32_t  objetivo_um;           /* −1 = sin objetivo                          */
    int32_t  pos_um;                /* −1 = posición desconocida                  */
    char     eje[16];               /* estado del eje                             */
    bool     ir_inicio, ir_fin;
    bool     armado;
    char     prox[8];               /* IR de proximidad: "nada", "lejos", "ok", "cerca" o "--" */

    char     mensaje[41];           /* línea de mensajes (vacía = pista de uso)   */
    char     error[41];             /* vacía si no hay error                      */

    bool     hay_negro, hay_blanco;
    uint32_t recorrido_ms;          /* calibración del eje, 0 = sin calibrar      */

    const historial_t *historial;
} ui_modelo_t;

/* Entradas ya interpretadas por la aplicación. */
typedef struct {
    bool b1, b2, b3;
    bool b4;            /* pulsación CORTA de B4, reconocida al SOLTAR        */
    bool b4_largo;      /* B4 mantenido ≥ 0,8 s                               */
    bool ok;            /* SW del joystick                                    */
    int  dir;           /* −1 = arriba, +1 = abajo, 0 = nada (un pulso por movimiento) */
    int  joy_x;         /* −100..100, continuo (se usa en "Mover cursor")      */
} ui_entrada_t;

typedef enum {
    UI_ACC_NINGUNA,
    UI_ACC_MEDIR,
    UI_ACC_CAL_NEGRO,
    UI_ACC_CAL_BLANCO,
    UI_ACC_REFERENCIAR,
    UI_ACC_IR_A,            /* mover el cursor a orden.um                      */
    UI_ACC_GUARDAR,
    UI_ACC_ARMAR,
    UI_ACC_DESARMAR,
    UI_ACC_MANUAL           /* entrar en modo manual (joystick mueve el cursor) */
} ui_accion_t;

typedef struct {
    ui_accion_t accion;
    int32_t     um;
} ui_orden_t;

void ui_iniciar(void);

/* Interpreta las entradas según la pantalla actual y devuelve la orden. */
ui_orden_t ui_procesar(const ui_entrada_t *entrada, const ui_modelo_t *modelo);

/* Pinta una sección de la pantalla actual. */
void ui_dibujar(const ui_modelo_t *modelo);

ui_pantalla_t ui_pantalla(void);

#endif /* INTERFAZ_H */
