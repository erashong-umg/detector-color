/*
 * deteccion.h — Caso de uso "detectar el color y apuntarlo en la escala".
 *
 *   medir (promedio de DETECCION_MUESTRAS) → normalizar con la calibración
 *   de negro/blanco → HEX y HSV → clasificar y situar en la escala →
 *   (opcional) mover el cursor con el eje.
 *
 * La calibración de color vive en RAM: se pierde al reiniciar (guardarla en
 * flash queda para la etapa de historial).
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
    DET_MOVIENDO,           /* el cursor va hacia el color                   */
    DET_YA_APUNTADO,        /* ya estaba dentro de la zona muerta            */
    DET_ACROMATICO,         /* negro / gris / blanco: no se mueve            */
    DET_NO_VALIDA,          /* lectura inválida                              */
    DET_SIN_REFERENCIA,     /* el eje no está referenciado                   */
    DET_NO_PUEDE            /* el eje no acepta la orden (desarmado, error)  */
} det_resultado_t;

void deteccion_iniciar(void);

/* Mide el color de la muestra que haya delante del sensor. */
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
