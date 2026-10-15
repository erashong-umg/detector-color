/*
 * posicion.h — Posición por tiempo del carrito, sin hardware.
 *
 * Se integra la velocidad aplicada: acum += velocidad(%) × dt(µs) (positiva
 * hacia el FINAL). Al calibrar, el acumulado entre INICIO y FINAL fija la
 * escala:
 *     posición = acum / acum_total × longitud
 * No hace falta conocer los mm/s: la rampa y la zona muerta quedan dentro
 * de la medición, y la calibración y los viajes integran lo mismo.
 *
 * El INICIO fija acum = 0 y el FINAL fija acum = acum_total (resincronización),
 * de modo que el error no se acumula de un viaje a otro.
 */
#ifndef POSICION_H
#define POSICION_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int64_t acum;           /* % · µs desde el inicio                       */
    int64_t total;          /* acum medido entre inicio y final             */
    int32_t longitud_um;
    bool    calibrada;
} posicion_t;

void posicion_iniciar(posicion_t *p, int32_t longitud_um);
void posicion_en_inicio(posicion_t *p);                     /* acum = 0           */
void posicion_en_fin(posicion_t *p);                        /* acum = total (si calibrada) */
void posicion_integrar(posicion_t *p, int velocidad, uint32_t dt_us);

/* Fija la escala con el acumulado actual (debe estar en el FINAL).
   Devuelve false si el acumulado no es positivo. */
bool posicion_calibrar(posicion_t *p);

/* Posición en µm, acotada a 0..longitud; −1 si no está calibrada. */
int32_t posicion_um(const posicion_t *p);

/* Velocidad a ordenar para ir de `actual` a `objetivo` (µm): 0 si ya llegó
   (|error| ≤ tol), velocidad de viaje lejos, de aproximación cerca. */
int posicion_comando(int32_t actual_um, int32_t objetivo_um,
                     int vel_viaje, int vel_aprox,
                     int32_t dist_aprox_um, int32_t tol_um);

#endif /* POSICION_H */
