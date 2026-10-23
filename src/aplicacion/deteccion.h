/*
 * deteccion.h — Caso de uso "detectar el color y llevar el carrito a su temperatura".
 *
 *   medir (promedio de DETECCION_MUESTRAS) → normalizar con la calibración
 *   de negro/blanco → HEX y HSV → clasificar (cálido / frío / neutro) →
 *   (opcional) mover el carrito a la posición de esa temperatura con el eje.
 *
 * La calibración de color vive en RAM: se pierde al reiniciar (guardarla en
 * flash queda para la etapa de historial).
 *
 * La adquisición NO bloquea: usa la interrupción de ALARM0 (drivers/tcs3200_async). Se pide con
 * deteccion_empezar() y se consulta con deteccion_paso() en cada vuelta del bucle principal
 * (≈ DETECCION_MUESTRAS × 88 ms). deteccion_medir() y deteccion_calibrar_*() quedan como
 * versiones que esperan, para las pruebas de banco.
 */
#ifndef DETECCION_H
#define DETECCION_H

#include <stdbool.h>
#include <stdint.h>
#include "dominio/color.h"
#include "dominio/escala.h"

typedef struct {
    bool                valida;     /* false: desborde del sensor o sin calibrar */
    color_frecuencias_t f;          /* frecuencias medias (Hz) */
    color_rgb_t         rgb;
    char                hex[8];
    color_hsv_t         hsv;
    escala_t            escala;
} lectura_t;

typedef enum {
    DET_MOVIENDO,           /* el carrito va hacia la posición de su temperatura */
    DET_YA_APUNTADO,        /* ya estaba dentro de la zona muerta            */
    DET_NO_VALIDA,          /* lectura inválida                              */
    DET_SIN_REFERENCIA,     /* el eje no está referenciado                   */
    DET_NO_PUEDE            /* el eje no acepta la orden (desarmado, error)  */
} det_resultado_t;

typedef enum {
    DET_TAREA_MEDIR,
    DET_TAREA_CAL_NEGRO,
    DET_TAREA_CAL_BLANCO
} det_tarea_t;

typedef enum {
    DET_PENDIENTE,      /* faltan juegos de medidas: volver a llamar a deteccion_paso()          */
    DET_LISTO,          /* terminó bien (MEDIR: *lectura rellena; CAL_*: calibración guardada)   */
    DET_FALLO           /* desborde, sin señal, sin calibrar o tarea no iniciada                 */
} det_progreso_t;

void deteccion_iniciar(void);

/* Función que se llama sin parar mientras esperan deteccion_medir() / deteccion_calibrar_*(). */
void deteccion_poner_espera(void (*hook)(void));

/* Arranca la adquisición por interrupción. false si el carrito no está quieto o ya hay una tarea. */
bool deteccion_empezar(det_tarea_t tarea);

/* Recoge los juegos que haya listos. Al terminar (LISTO o FALLO) detiene la adquisición.
   *lectura solo se rellena con la tarea MEDIR. */
det_progreso_t deteccion_paso(lectura_t *lectura);

/* Aborta la tarea en curso y detiene la adquisición. */
void deteccion_cancelar(void);
bool deteccion_ocupada(void);

/* Versiones que esperan hasta terminar (llaman al hook de espera). */
bool deteccion_medir(lectura_t *lectura);

/* Calibración de dos puntos: muestra negra mate / papel blanco. */
bool deteccion_calibrar_negro(void);
bool deteccion_calibrar_blanco(void);
void deteccion_borrar_calibracion(void);
bool deteccion_calibrado(void);     /* negro Y blanco */
bool deteccion_hay_negro(void);
bool deteccion_hay_blanco(void);

/* Envía el cursor a la posición de la lectura, si procede. */
det_resultado_t deteccion_apuntar(const lectura_t *lectura);
const char     *deteccion_resultado_texto(det_resultado_t r);

#endif /* DETECCION_H */
