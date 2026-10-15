/*
 * finales.h — Sensores IR de los extremos del recorrido (salida digital).
 *
 * "Activo" significa extremo alcanzado, sea cual sea la polaridad eléctrica
 * (FINAL_ACTIVO_BAJO). Antirrebote por tiempo. El pull interno apunta hacia
 * el estado activo: un cable suelto se lee como extremo alcanzado.
 * finales_actualizar() debe llamarse en cada vuelta del bucle (≤ 1 ms).
 */
#ifndef FINALES_H
#define FINALES_H

#include <stdbool.h>

void finales_iniciar(void);
void finales_actualizar(void);

bool final_inicio_activo(void);
bool final_fin_activo(void);

/* Nivel eléctrico crudo del pin (sin antirrebote ni polaridad), para diagnóstico. */
bool final_inicio_crudo(void);
bool final_fin_crudo(void);

#endif /* FINALES_H */
