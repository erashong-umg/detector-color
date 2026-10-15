#include "dominio/historial.h"

void historial_iniciar(historial_t *h)
{
    h->cantidad = 0;
    h->siguiente = 0;
}

void historial_agregar(historial_t *h, const entrada_hist_t *e)
{
    h->entradas[h->siguiente] = *e;
    h->siguiente = (uint8_t)((h->siguiente + 1u) % HISTORIAL_MAX);
    if (h->cantidad < HISTORIAL_MAX) {
        h->cantidad++;
    }
}

uint8_t historial_cantidad(const historial_t *h)
{
    return h->cantidad;
}

bool historial_obtener(const historial_t *h, uint8_t indice, entrada_hist_t *e)
{
    if (indice >= h->cantidad) {
        return false;
    }
    /* La última escrita está justo antes de `siguiente`. */
    unsigned pos = ((unsigned)h->siguiente + HISTORIAL_MAX - 1u - indice) % HISTORIAL_MAX;
    *e = h->entradas[pos];
    return true;
}
