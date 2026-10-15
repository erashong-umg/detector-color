#include "aplicacion/deteccion.h"

#include "aplicacion/eje.h"
#include "configuracion/escala.h"
#include "drivers/tcs3200.h"

static color_calibracion_t cal;

void deteccion_iniciar(void)
{
    tcs3200_iniciar();
    deteccion_borrar_calibracion();
}

/* Promedio de n mediciones. false si alguna desbordó el contador. */
static bool medir_promedio(color_frecuencias_t *f)
{
    uint64_t r = 0, g = 0, b = 0, c = 0;

    for (int i = 0; i < DETECCION_MUESTRAS; i++) {
        tcs_frecuencias_t m;
        if (!tcs3200_medir(&m)) {
            return false;
        }
        r += m.r; g += m.g; b += m.b; c += m.c;
    }
    f->r = (uint32_t)(r / DETECCION_MUESTRAS);
    f->g = (uint32_t)(g / DETECCION_MUESTRAS);
    f->b = (uint32_t)(b / DETECCION_MUESTRAS);
    f->c = (uint32_t)(c / DETECCION_MUESTRAS);
    return true;
}

bool deteccion_medir(lectura_t *l)
{
    l->valida = false;
    l->escala.clase = ESCALA_NEGRO;
    l->escala.tiene_posicion = false;
    l->escala.x_um = 0;

    /* Medir bloquea ≈ 270 ms sin vigilar los finales: solo con el carrito parado. */
    if (!eje_quieto() || !medir_promedio(&l->f)) {
        return false;
    }
    if (!color_normalizar(&cal, &l->f, &l->rgb)) {
        return false;
    }
    color_a_hex(l->rgb, l->hex);
    l->hsv = color_a_hsv(l->rgb);
    l->escala = escala_mapear(l->hsv, eje_posicion_um());
    l->valida = true;
    return true;
}

bool deteccion_calibrar_negro(void)
{
    if (!eje_quieto() || !medir_promedio(&cal.negro)) {
        return false;
    }
    cal.hay_negro = true;
    return true;
}

bool deteccion_calibrar_blanco(void)
{
    if (!eje_quieto() || !medir_promedio(&cal.blanco)) {
        return false;
    }
    cal.hay_blanco = true;
    return true;
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
    if (!l->escala.tiene_posicion) {
        return DET_ACROMATICO;
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
    case DET_MOVIENDO:        return "moviendo el cursor";
    case DET_YA_APUNTADO:     return "el cursor ya apunta a ese color";
    case DET_ACROMATICO:      return "color sin tono (negro/gris/blanco): el cursor no se mueve";
    case DET_NO_VALIDA:       return "lectura no valida (desborde o sin calibrar)";
    case DET_SIN_REFERENCIA:  return "el eje no esta referenciado";
    case DET_NO_PUEDE:        return "el eje no acepta la orden (desarmado o en error)";
    }
    return "?";
}
