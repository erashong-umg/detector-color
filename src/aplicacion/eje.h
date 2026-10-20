/*
 * eje.h — Eje lineal del carrito: homing, calibración y posición por tiempo.
 *
 * Encima de carro.h (que impone los límites de seguridad). Se encarga de:
 *   - HOMING: busca el final INICIO despacio y fija ahí la posición 0;
 *   - CALIBRACIÓN: recorre hasta el final FIN midiendo el recorrido (escala);
 *   - volver al INICIO y quedar LISTO;
 *   - ir a una posición en µm, o a los extremos hasta el final de carrera real;
 *   - jog manual con el joystick;
 *   - resincronizar la posición cada vez que toca un final de carrera.
 *
 * eje_actualizar() llama a carro_actualizar(): basta con llamarla en cada
 * vuelta del bucle (≤ 1 ms). Posición en micrómetros; 0 = INICIO.
 */
#ifndef EJE_LINEAL_H
#define EJE_LINEAL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    EJE_SIN_REFERENCIA,     /* tras el arranque: posición desconocida        */
    EJE_BUSCANDO_INICIO,    /* homing                                        */
    EJE_CALIBRANDO,         /* recorriendo hacia el FINAL midiendo           */
    EJE_MOVIENDO,           /* yendo a un destino                            */
    EJE_LISTO,              /* referenciado y detenido                       */
    EJE_ERROR
} eje_estado_t;

typedef enum {
    EJE_ERROR_NINGUNO,
    EJE_ERROR_CARRO,                /* error del supervisor (timeout, finales) */
    EJE_ERROR_SENTIDO_INVERTIDO,    /* se activó el final contrario al buscado */
    EJE_ERROR_RECORRIDO_CORTO       /* calibración demasiado corta             */
} eje_error_t;

/* carro_iniciar() debe haberse llamado antes. */
void eje_iniciar(void);
void eje_actualizar(void);

/* Inicia homing + calibración + vuelta al inicio. Requiere carro armado. */
bool eje_referenciar(void);

/* Va a x_um (0..longitud). Los extremos (≤ tolerancia / ≥ longitud − tolerancia)
   se alcanzan hasta el final de carrera real. false si no está referenciado. */
bool eje_ir_a(int32_t x_um);

/* Jog manual: velocidad −100..100 (+ = hacia el FINAL). Solo en LISTO o
   SIN_REFERENCIA; el eje sigue estimando la posición. 0 = parar. */
void eje_manual(int velocidad);

void eje_parar(void);
bool eje_reiniciar(void);   /* limpia el error; pide referenciar de nuevo */

eje_estado_t eje_estado(void);
eje_error_t  eje_error(void);
const char  *eje_estado_texto(eje_estado_t e);
const char  *eje_error_texto(eje_error_t e);

bool    eje_calibrado(void);
int32_t eje_posicion_um(void);      /* −1 si no está calibrado */
bool    eje_en_destino(void);       /* LISTO */

/* ¿Está el carrito parado de verdad (motor a 0, sin movimiento en curso)?
   Las operaciones que bloquean (medir el color tarda ≈ 270 ms sin llamar a
   eje_actualizar) solo son seguras en ese caso. */
bool    eje_quieto(void);
uint32_t eje_recorrido_ms(void);    /* duración medida de la calibración */
uint32_t eje_ultima_calibracion_ms(void);   /* la última pasada INICIO→FIN medida, aunque haya fallado (diagnóstico) */

#endif /* EJE_LINEAL_H */
