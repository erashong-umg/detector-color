#include "dominio/posicion.h"

void posicion_iniciar(posicion_t *p, int32_t longitud_um)
{
    p->acum = 0;
    p->total = 0;
    p->longitud_um = longitud_um;
    p->calibrada = false;
}

void posicion_en_inicio(posicion_t *p)
{
    p->acum = 0;
}

void posicion_en_fin(posicion_t *p)
{
    if (p->calibrada) {
        p->acum = p->total;
    }
}

void posicion_integrar(posicion_t *p, int velocidad, uint32_t dt_us)
{
    p->acum += (int64_t)velocidad * (int64_t)dt_us;
}

bool posicion_calibrar(posicion_t *p)
{
    if (p->acum <= 0) {
        return false;
    }
    p->total = p->acum;
    p->calibrada = true;
    return true;
}

int32_t posicion_um(const posicion_t *p)
{
    if (!p->calibrada) {
        return -1;
    }
    if (p->acum <= 0) {
        return 0;
    }
    if (p->acum >= p->total) {
        return p->longitud_um;
    }
    return (int32_t)(p->acum * p->longitud_um / p->total);
}

int posicion_comando(int32_t actual_um, int32_t objetivo_um,
                     int vel_viaje, int vel_aprox,
                     int32_t dist_aprox_um, int32_t tol_um)
{
    int32_t resto = objetivo_um - actual_um;
    int32_t dist = resto < 0 ? -resto : resto;

    if (dist <= tol_um) {
        return 0;
    }
    int v = dist > dist_aprox_um ? vel_viaje : vel_aprox;
    return resto > 0 ? v : -v;
}
