/*
 * sistema.h — Parámetros de la máquina de estados.
 */
#ifndef CONFIG_SISTEMA_H
#define CONFIG_SISTEMA_H

/* Tras pedir medir o calibrar, tiempo máximo esperando a que el objeto esté a
   la distancia correcta. Pasado, se cancela. */
#define SISTEMA_ESPERA_OBJETO_MS    8000u

/* El objeto debe estar a la distancia correcta de forma continua este tiempo
   antes de medir (evita medir mientras se está colocando). */
#define SISTEMA_PROX_ESTABLE_MS     300u

#endif /* CONFIG_SISTEMA_H */
