/*
 * carro.h — Supervisor de seguridad del carrito (motor + finales de carrera).
 *
 * Es la ÚNICA vía por la que el resto del firmware mueve el motor. Aplica:
 *   - la regla de límites (dominio/limites.c) con los finales de carrera;
 *   - freno inmediato al tocar un extremo;
 *   - tiempo máximo de movimiento continuo (CARRO_TIMEOUT_MS);
 *   - fallo si los dos finales están activos a la vez.
 * Un error se mantiene (motor frenado) hasta carro_reiniciar_error().
 *
 * Convención: velocidad > 0 = hacia el FINAL, < 0 = hacia el INICIO.
 * carro_actualizar() debe llamarse en cada vuelta del bucle (≤ 1 ms).
 */
#ifndef CARRO_H
#define CARRO_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CARRO_ERROR_NINGUNO,
    CARRO_ERROR_FINALES_INCOHERENTES,   /* ambos finales activos           */
    CARRO_ERROR_TIMEOUT                 /* demasiado tiempo en movimiento  */
} carro_error_t;

/* Deja el motor en standby (desarmado). */
void carro_iniciar(void);

/* Arma o desarma el puente. Armar falla si hay un error activo. */
bool carro_armar(bool armar);
bool carro_armado(void);

/* Velocidad pedida (−100..100). Se ignora si no está armado o hay error. */
void carro_mover(int velocidad);
void carro_parar(void);         /* parada con rampa  */
void carro_frenar(void);        /* freno inmediato   */

/* Tiempo máximo de movimiento continuo; por defecto CARRO_TIMEOUT_MS. */
void carro_timeout_ms(uint32_t ms);

void carro_actualizar(void);

carro_error_t carro_error(void);
const char   *carro_error_texto(carro_error_t e);
bool          carro_reiniciar_error(void);  /* false si la causa persiste */

/* Velocidad realmente aplicada (con rampa y filtro de límites). */
int  carro_velocidad_real(void);

/* ¿Se bloqueó el último movimiento pedido por un final de carrera? */
bool carro_bloqueado_por_final(void);

#endif /* CARRO_H */
