#include "dominio/selector.h"

#include "configuracion/escala.h"

void selector_iniciar(selector_t *sel)
{
    sel->h_x1000 = 0;
    sel->s = 255;
}

void selector_mover(selector_t *sel, int joy_x, int dir)
{
    sel->h_x1000 += (int32_t)joy_x * SELECTOR_PASO_TONO_X1000;
    sel->h_x1000 %= 360000;
    if (sel->h_x1000 < 0) {
        sel->h_x1000 += 360000;
    }

    if (dir != 0) {
        sel->s -= dir * SELECTOR_PASO_SATURACION;       /* arriba (−1) = más saturado */
        if (sel->s < 0)   { sel->s = 0; }
        if (sel->s > 255) { sel->s = 255; }
    }
}

color_hsv_t selector_hsv(const selector_t *sel)
{
    color_hsv_t hsv;

    hsv.h = (int16_t)(sel->h_x1000 / 1000);
    hsv.s = (uint8_t)sel->s;
    hsv.v = SELECTOR_VALOR;
    return hsv;
}
