#include "dominio/escala.h"

#include "configuracion/escala.h"
#include "configuracion/eje.h"

_Static_assert(ESCALA_DESPLAZAMIENTO_UM + ESCALA_ANCHO_UM <= EJE_LONGITUD_UM,
               "escala.c: la escala impresa no cabe en el recorrido del carrito (EJE_LONGITUD_UM)");

static int32_t distancia(int32_t a, int32_t b)
{
    return a > b ? a - b : b - a;
}

int32_t escala_um_de_temperatura(temperatura_t t)
{
    switch (t) {
    case TEMP_CALIDO: return ESCALA_POS_CALIDO_UM;
    case TEMP_FRIO:   return ESCALA_POS_FRIO_UM;
    case TEMP_NEUTRO: break;
    }
    return ESCALA_POS_NEUTRO_UM;
}

temperatura_t escala_temperatura_de_tono(int h)
{
    if (h < 0) {
        return TEMP_NEUTRO;
    }
    h %= 360;
    return (h >= ESCALA_TONO_FRIO_DESDE && h < ESCALA_TONO_CALIDO_DESDE) ? TEMP_FRIO : TEMP_CALIDO;
}

escala_t escala_mapear(color_hsv_t hsv)
{
    escala_t r;

    /* Acromáticos: el tono no tiene significado (en negro es solo ruido). */
    if (hsv.v < ESCALA_VAL_NEGRO) {
        r.clase = ESCALA_NEGRO;
    } else if (hsv.h < 0 || hsv.s < ESCALA_SAT_MIN) {
        r.clase = hsv.v >= ESCALA_VAL_BLANCO ? ESCALA_BLANCO : ESCALA_GRIS;
    } else {
        r.clase = ESCALA_CROMATICO;
    }

    r.temperatura = r.clase == ESCALA_CROMATICO ? escala_temperatura_de_tono(hsv.h) : TEMP_NEUTRO;
    r.x_um = escala_um_de_temperatura(r.temperatura);
    return r;
}

bool escala_hay_que_mover(int32_t actual_um, int32_t objetivo_um)
{
    if (actual_um < 0) {
        return true;
    }
    return distancia(actual_um, objetivo_um) > ESCALA_ZONA_MUERTA_UM;
}

const char *escala_temperatura_texto(temperatura_t t)
{
    switch (t) {
    case TEMP_CALIDO: return "cálido";
    case TEMP_NEUTRO: return "neutro";
    case TEMP_FRIO:   return "frío";
    }
    return "?";
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
