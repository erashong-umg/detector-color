#include "dominio/eje.h"

int eje_normalizar(uint16_t crudo, uint16_t centro, int zona_muerta, bool invertir)
{
    int v;

    if (crudo >= centro) {
        int rango = EJE_ADC_MAX - (int)centro;
        v = rango > 0 ? ((int)(crudo - centro) * 100) / rango : 0;
    } else {
        v = centro > 0 ? -(((int)(centro - crudo) * 100) / (int)centro) : 0;
    }

    if (v > 100)  { v = 100; }
    if (v < -100) { v = -100; }

    /* Zona muerta con reescalado: [zm..100] → [0..100] */
    int mag = v < 0 ? -v : v;
    if (mag <= zona_muerta) {
        v = 0;
    } else {
        mag = (mag - zona_muerta) * 100 / (100 - zona_muerta);
        v = v < 0 ? -mag : mag;
    }

    return invertir ? -v : v;
}
