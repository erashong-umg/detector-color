#include "dominio/proximidad.h"

#include "configuracion/proximidad.h"

void prox_filtro_iniciar(prox_filtro_t *f)
{
    f->estado = PROX_SIN_OBJETO;
    f->candidato = PROX_SIN_OBJETO;
    f->cuenta = 0;
}

prox_estado_t prox_clasificar(uint16_t nivel, bool digital_activo, prox_estado_t actual)
{
    /* umbral[i] separa la banda i de la i + 1 */
    static const int umbral[3] = { PROX_NIVEL_PRESENCIA, PROX_NIVEL_OK, PROX_NIVEL_CERCA };
    int banda = (int)actual;
    int n = (int)nivel;

    /* Se sube de banda solo al superar el umbral + histéresis, y se baja
       solo al quedar por debajo del umbral − histéresis. */
    while (banda < 3 && n >= umbral[banda] + PROX_HISTERESIS) {
        banda++;
    }
    while (banda > 0 && n < umbral[banda - 1] - PROX_HISTERESIS) {
        banda--;
    }
    if (digital_activo && banda < (int)PROX_LEJOS) {
        banda = (int)PROX_LEJOS;
    }
    return (prox_estado_t)banda;
}

void prox_filtro_actualizar(prox_filtro_t *f, uint16_t nivel, bool digital_activo)
{
    prox_estado_t nuevo = prox_clasificar(nivel, digital_activo, f->estado);

    if (nuevo == f->estado) {
        f->candidato = f->estado;
        f->cuenta = 0;
    } else if (nuevo == f->candidato) {
        if (++f->cuenta >= PROX_MUESTRAS_ESTABLES) {
            f->estado = nuevo;
            f->cuenta = 0;
        }
    } else {
        f->candidato = nuevo;
        f->cuenta = 1;
    }
}

const char *prox_texto(prox_estado_t e)
{
    switch (e) {
    case PROX_SIN_OBJETO: return "nada";
    case PROX_LEJOS:      return "lejos";
    case PROX_OK:         return "ok";
    case PROX_CERCA:      return "cerca";
    }
    return "--";
}
