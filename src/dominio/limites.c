#include "dominio/limites.h"

bool limites_incoherentes(bool inicio_activo, bool fin_activo)
{
    return inicio_activo && fin_activo;
}

int limites_filtrar(int velocidad, bool inicio_activo, bool fin_activo)
{
    if (limites_incoherentes(inicio_activo, fin_activo)) {
        return 0;
    }
    if (fin_activo && velocidad > 0) {
        return 0;
    }
    if (inicio_activo && velocidad < 0) {
        return 0;
    }
    return velocidad;
}
