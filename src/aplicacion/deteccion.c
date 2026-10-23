#include "aplicacion/deteccion.h"

#include <stddef.h>

#include "aplicacion/eje.h"
#include "configuracion/escala.h"
#include "configuracion/tcs3200.h"
#include "drivers/tcs3200.h"
#include "drivers/tcs3200_async.h"
#include "hw/tiempo.h"

static color_calibracion_t cal;

static void (*hook_espera)(void);

/* Tarea en curso: promedio de DETECCION_MUESTRAS juegos que llegan desde la ISR. */
static bool         activa;
static det_tarea_t  tarea;
static int          juegos;
static uint64_t     suma_r, suma_g, suma_b, suma_c;

void deteccion_poner_espera(void (*hook)(void))
{
    hook_espera = hook;
}

void deteccion_iniciar(void)
{
    tcs3200_iniciar();
    activa = false;
    deteccion_borrar_calibracion();
}

bool deteccion_ocupada(void)
{
    return activa;
}

bool deteccion_empezar(det_tarea_t t)
{
    if (activa || !eje_quieto()) {
        return false;
    }
    tarea = t;
    juegos = 0;
    suma_r = suma_g = suma_b = suma_c = 0;
    activa = true;
    tcs3200_async_iniciar();
    return true;
}

void deteccion_cancelar(void)
{
    if (activa) {
        tcs3200_async_detener();
        activa = false;
    }
}

/* Con las frecuencias medias ya calculadas: rellena la lectura (tarea MEDIR). */
static bool completar_lectura(const color_frecuencias_t *f, lectura_t *l)
{
    l->valida = false;
    l->escala.clase = ESCALA_NEGRO;
    l->escala.temperatura = TEMP_NEUTRO;
    l->escala.x_um = -1;
    l->f = *f;

    if (!color_normalizar(&cal, &l->f, &l->rgb)) {
        return false;
    }
    color_a_hex(l->rgb, l->hex);
    l->hsv = color_a_hsv(l->rgb);
    l->escala = escala_mapear(l->hsv);
    l->valida = true;
    return true;
}

det_progreso_t deteccion_paso(lectura_t *l)
{
    tcs_frecuencias_t m;
    bool valido;

    if (!activa) {
        return DET_FALLO;
    }
    if (!tcs3200_async_tomar(&m, &valido)) {
        return DET_PENDIENTE;
    }
    if (!valido) {                          /* el contador de 16 bits dio la vuelta */
        deteccion_cancelar();
        return DET_FALLO;
    }

    suma_r += m.r; suma_g += m.g; suma_b += m.b; suma_c += m.c;
    if (++juegos < DETECCION_MUESTRAS) {
        return DET_PENDIENTE;
    }
    deteccion_cancelar();                   /* ya hay todos los juegos: detiene la ISR */

    color_frecuencias_t f = {
        (uint32_t)(suma_r / DETECCION_MUESTRAS),
        (uint32_t)(suma_g / DETECCION_MUESTRAS),
        (uint32_t)(suma_b / DETECCION_MUESTRAS),
        (uint32_t)(suma_c / DETECCION_MUESTRAS),
    };
    /* Sin señal en el canal claro: el sensor está desconectado o sin alimentación. */
    if (f.c < TCS_FRECUENCIA_MIN_HZ) {
        return DET_FALLO;
    }

    switch (tarea) {
    case DET_TAREA_CAL_NEGRO:
        cal.negro = f;
        cal.hay_negro = true;
        return DET_LISTO;
    case DET_TAREA_CAL_BLANCO:
        cal.blanco = f;
        cal.hay_blanco = true;
        return DET_LISTO;
    case DET_TAREA_MEDIR:
    default:
        return completar_lectura(&f, l) ? DET_LISTO : DET_FALLO;
    }
}

/* Versión que espera: para las pruebas de banco (no se usa en la aplicación). */
static bool esperar_tarea(det_tarea_t t, lectura_t *l)
{
    lectura_t descartada;

    if (!deteccion_empezar(t)) {
        return false;
    }
    for (;;) {
        det_progreso_t r = deteccion_paso(l ? l : &descartada);
        if (r != DET_PENDIENTE) {
            return r == DET_LISTO;
        }
        if (hook_espera) {
            hook_espera();
        }
        tiempo_esperar_us(100);
    }
}

bool deteccion_medir(lectura_t *l)
{
    return esperar_tarea(DET_TAREA_MEDIR, l);
}

bool deteccion_calibrar_negro(void)
{
    return esperar_tarea(DET_TAREA_CAL_NEGRO, NULL);
}

bool deteccion_calibrar_blanco(void)
{
    return esperar_tarea(DET_TAREA_CAL_BLANCO, NULL);
}

void deteccion_borrar_calibracion(void)
{
    cal.hay_negro = cal.hay_blanco = false;
}

bool deteccion_calibrado(void)
{
    return color_calibrado(&cal);
}

bool deteccion_hay_negro(void)
{
    return cal.hay_negro;
}

bool deteccion_hay_blanco(void)
{
    return cal.hay_blanco;
}

det_resultado_t deteccion_apuntar(const lectura_t *l)
{
    if (!l->valida) {
        return DET_NO_VALIDA;
    }
    if (!eje_calibrado()) {
        return DET_SIN_REFERENCIA;
    }
    if (!escala_hay_que_mover(eje_posicion_um(), l->escala.x_um)) {
        return DET_YA_APUNTADO;
    }
    return eje_ir_a(l->escala.x_um) ? DET_MOVIENDO : DET_NO_PUEDE;
}

const char *deteccion_resultado_texto(det_resultado_t r)
{
    switch (r) {
    case DET_MOVIENDO:        return "moviendo el carrito";
    case DET_YA_APUNTADO:     return "el carrito ya esta en esa posicion";
    case DET_NO_VALIDA:       return "lectura no valida (desborde o sin calibrar)";
    case DET_SIN_REFERENCIA:  return "el eje no esta referenciado";
    case DET_NO_PUEDE:        return "el eje no acepta la orden (desarmado o en error)";
    }
    return "?";
}
