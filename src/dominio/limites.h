/*
 * limites.h — Regla de seguridad del carrito, sin hardware.
 *
 * Convención: velocidad > 0 = hacia el FINAL, < 0 = hacia el INICIO.
 *
 * Con un extremo activo solo se permite alejarse de él. Con los dos activos
 * a la vez (cable suelto, sensor mal configurado, carrito atascado entre
 * ambos) no se permite ningún movimiento.
 */
#ifndef LIMITES_H
#define LIMITES_H

#include <stdbool.h>

/* Devuelve la velocidad que se puede aplicar de verdad (0 si está bloqueada). */
int limites_filtrar(int velocidad, bool inicio_activo, bool fin_activo);

/* ¿Es incoherente el estado de los finales? (ambos activos) */
bool limites_incoherentes(bool inicio_activo, bool fin_activo);

#endif /* LIMITES_H */
