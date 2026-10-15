#include "dominio/escala.h"

#include "configuracion/escala.h"
#include "configuracion/eje.h"

_Static_assert(ESCALA_DESPLAZAMIENTO_UM + ESCALA_ANCHO_UM <= EJE_LONGITUD_UM,
               "escala.c: la escala impresa no cabe en el recorrido del carrito (EJE_LONGITUD_UM)");

int32_t escala_um_de_tono(int h)
{
    int rel = ((h - ESCALA_TONO_INICIAL_GRADOS) % 360 + 360) % 360;
    return ESCALA_DESPLAZAMIENTO_UM + (int32_t)((int64_t)rel * ESCALA_ANCHO_UM / 360);
}

static int32_t distancia(int32_t a, int32_t b)
{
    return a > b ? a - b : b - a;
}

escala_t escala_mapear(color_hsv_t hsv, int32_t actual_um)
{
    escala_t r;
    r.x_um = 0;
    r.tiene_posicion = false;

    /* Acromáticos: el tono no tiene significado (en negro es solo ruido). */
    if (hsv.v < ESCALA_VAL_NEGRO) {
        r.clase = ESCALA_NEGRO;
    } else if (hsv.h < 0 || hsv.s < ESCALA_SAT_MIN) {
        r.clase = hsv.v >= ESCALA_VAL_BLANCO ? ESCALA_BLANCO : ESCALA_GRIS;
    } else {
        r.clase = ESCALA_CROMATICO;
    }

    if (r.clase != ESCALA_CROMATICO) {
        if (ESCALA_ACROMATICO_AL_INICIO) {
            r.tiene_posicion = true;
            r.x_um = 0;
        }
        return r;
    }

    r.tiene_posicion = true;
    r.x_um = escala_um_de_tono(hsv.h);

    /* Banda del rojo: ambos extremos están impresos en rojo, así que se va al
       más cercano. Un tono ligeramente por encima de 0° equivale, en el extremo
       derecho, a "rojo justo pasado el cierre" (el propio extremo); uno ligeramente
       por debajo de 360° equivale al extremo izquierdo. */
    int rel = ((hsv.h - ESCALA_TONO_INICIAL_GRADOS) % 360 + 360) % 360;
    if (actual_um >= 0 && (rel < ESCALA_BANDA_ROJO_GRADOS ||
                           rel > 360 - ESCALA_BANDA_ROJO_GRADOS)) {
        int32_t alterna = rel < ESCALA_BANDA_ROJO_GRADOS
                              ? ESCALA_DESPLAZAMIENTO_UM + ESCALA_ANCHO_UM
                              : ESCALA_DESPLAZAMIENTO_UM;
        if (distancia(actual_um, alterna) < distancia(actual_um, r.x_um)) {
            r.x_um = alterna;
        }
    }
    return r;
}

bool escala_hay_que_mover(int32_t actual_um, int32_t objetivo_um)
{
    if (actual_um < 0) {
        return true;
    }
    return distancia(actual_um, objetivo_um) > ESCALA_ZONA_MUERTA_UM;
}

const char *escala_clase_texto(escala_clase_t c)
{
    switch (c) {
    case ESCALA_CROMATICO: return "cromatico";
    case ESCALA_NEGRO:     return "negro";
    case ESCALA_BLANCO:    return "blanco";
    case ESCALA_GRIS:      return "gris";
    }
    return "?";
}
