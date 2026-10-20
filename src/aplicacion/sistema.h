/*
 * sistema.h — Máquina de estados del detector de color.
 *
 *   INIT → IDLE ⇄ HOMING            referenciar el eje (busca inicio, calibra, vuelve)
 *               ⇄ MEASURE           espera el objeto, mide, apunta el cursor
 *               ⇄ CALIBRATE         espera el objeto, calibra negro o blanco
 *               ⇄ MOVE_TO_TARGET    el cursor va a una posición
 *               ⇄ MANUAL            movimiento con el joystick
 *       cualquiera → ERROR → (reiniciar) → IDLE
 *
 * Es el orquestador del dominio: usa el eje, el sensor de color y el IR de
 * proximidad, pero NO toca la pantalla, los botones, ni el buzzer.
 * La aplicación le da órdenes (sistema_*) y recoge eventos
 * (sistema_tomar_evento) y la guía al usuario (sistema_guia).
 *
 * sistema_actualizar() debe llamarse en cada vuelta del bucle (≤ 1 ms):
 * llama a eje_actualizar() y a proximidad_actualizar().
 *
 * Medir y calibrar el color esperan a que el IR de proximidad vea el objeto a
 * la distancia correcta (la distancia cambia la intensidad medida) y bloquean
 * ≈ 270 ms al medir, por eso solo ocurren con el carrito parado.
 */
#ifndef SISTEMA_H
#define SISTEMA_H

#include <stdbool.h>
#include <stdint.h>
#include "aplicacion/deteccion.h"
#include "dominio/historial.h"

typedef enum {
    SIS_INIT,
    SIS_HOMING,
    SIS_IDLE,
    SIS_MEASURE,
    SIS_CALIBRATE,
    SIS_MOVE_TO_TARGET,
    SIS_MANUAL,
    SIS_ERROR
} sistema_estado_t;

typedef enum {
    SIS_EVT_NINGUNO,
    SIS_EVT_LISTO,              /* terminó la inicialización                  */
    SIS_EVT_AUTOREFERENCIANDO,  /* el sistema armó el motor y empezó a referenciar solo */
    SIS_EVT_AUTOREF_REINTENTO,  /* la referencia automática falló y se reintenta (ver sistema_autoref_intentos) */
    SIS_EVT_REFERENCIADO,       /* homing y calibración del eje terminados     */
    SIS_EVT_MEDIDA,             /* lectura válida (ver sistema_apuntado)       */
    SIS_EVT_CAL_NEGRO,          /* negro calibrado                             */
    SIS_EVT_CAL_BLANCO,         /* blanco calibrado                            */
    SIS_EVT_LLEGADA,            /* el cursor llegó a su destino                */
    SIS_EVT_GUIA,               /* cambió la guía (ver sistema_guia)           */
    SIS_EVT_ANALIZANDO,         /* la muestra está lista: se va a medir ahora (≈ 270 ms) */
    SIS_EVT_TIEMPO_AGOTADO,     /* no se colocó el objeto a tiempo             */
    SIS_EVT_CANCELADO,
    SIS_EVT_LECTURA_INVALIDA,   /* desborde del sensor                         */
    SIS_EVT_ERROR,
    SIS_EVT_RECUPERADO,         /* los finales volvieron a ser coherentes: error reiniciado solo, se vuelve a referenciar */
    SIS_EVT_ERROR_REINICIADO
} sistema_evento_t;

/* Qué debe hacer el usuario con el objeto mientras se espera para medir. */
typedef enum {
    SIS_GUIA_NINGUNA,
    SIS_GUIA_COLOCAR_OBJETO,    /* no hay objeto                               */
    SIS_GUIA_ACERCAR,           /* está lejos                                  */
    SIS_GUIA_ALEJAR             /* está demasiado cerca                        */
} sistema_guia_t;

/* Por qué se rechazó la última orden. */
typedef enum {
    SIS_RECHAZO_NINGUNO,
    SIS_RECHAZO_OCUPADO,        /* hay otra operación en curso                 */
    SIS_RECHAZO_ERROR_ACTIVO,
    SIS_RECHAZO_NO_ARMADO,
    SIS_RECHAZO_SIN_REFERENCIA,
    SIS_RECHAZO_NO_CALIBRADO    /* falta calibrar negro y blanco               */
} sistema_rechazo_t;

void sistema_iniciar(void);
void sistema_actualizar(void);

/* ---- Órdenes: devuelven false si se rechazan (ver sistema_rechazo) ---- */
bool sistema_armar(bool armar);
bool sistema_referenciar(void);
bool sistema_medir(void);
bool sistema_calibrar(bool negro);
void sistema_forzar_objeto(void);               /* medir/calibrar ya, sin esperar al IR de proximidad */
bool sistema_ir_a(int32_t x_um);
bool sistema_manual(bool activar);
bool sistema_manual_ir_a(int32_t x_um);         /* dentro de MANUAL: lleva el carrito a una posición sin salir del modo */
void sistema_manual_velocidad(int velocidad);   /* −100..100, solo en MANUAL */
bool sistema_guardar(void);                     /* última lectura → historial */
void sistema_cancelar(void);                    /* aborta lo que esté en curso */
bool sistema_reiniciar_error(void);

/* ---- Consultas ---- */
sistema_estado_t   sistema_estado(void);
const char        *sistema_estado_texto(sistema_estado_t e);
sistema_evento_t   sistema_tomar_evento(void);          /* consume; NINGUNO si no hay */
sistema_guia_t     sistema_guia(void);
sistema_rechazo_t  sistema_rechazo(void);
det_resultado_t    sistema_apuntado(void);              /* qué pasó con el cursor tras medir */

bool               sistema_hay_lectura(void);
const lectura_t   *sistema_lectura(void);
int32_t            sistema_objetivo_um(void);           /* −1 si no hay                      */
const historial_t *sistema_historial(void);
bool               sistema_armado(void);
bool               sistema_referenciado(void);
uint8_t            sistema_autoref_intentos(void);      /* reintentos de la referencia automática hasta ahora */

#endif /* SISTEMA_H */
