#include "drivers/botones.h"

#include "configuracion/entradas.h"
#include "configuracion/pines.h"
#include "hw/pin.h"
#include "hw/tiempo.h"

typedef struct {
    uint8_t  pin;
    bool     activo_alto;   /* true = pulsado con el pin en alto */
    bool     estable;       /* nivel aceptado: true = pulsado */
    bool     candidato;     /* última lectura cruda           */
    uint32_t t_candidato;   /* desde cuándo se lee igual      */
    uint32_t t_pulsado;     /* instante en que se aceptó la pulsación */
    uint32_t duracion_us;
    bool     largo_avisado;
    bool     ev_pulsado, ev_soltado, ev_largo;
} boton_t;

static boton_t botones[BOTON_CANTIDAD] = {
    [BOTON_1]   = { .pin = PIN_BOTON_1, .activo_alto = BOTONES_ACTIVO_ALTO },
    [BOTON_2]   = { .pin = PIN_BOTON_2, .activo_alto = BOTONES_ACTIVO_ALTO },
    [BOTON_3]   = { .pin = PIN_BOTON_3, .activo_alto = BOTONES_ACTIVO_ALTO },
    [BOTON_4]   = { .pin = PIN_BOTON_4, .activo_alto = BOTONES_ACTIVO_ALTO },
    [BOTON_JOY] = { .pin = PIN_JOY_SW,  .activo_alto = JOY_SW_ACTIVO_ALTO  },
};

void botones_iniciar(void)
{
    uint32_t ahora = tiempo_us();

    for (int i = 0; i < BOTON_CANTIDAD; i++) {
        boton_t *b = &botones[i];
        pin_entrada(b->pin, b->activo_alto ? PIN_PULL_DOWN : PIN_PULL_UP);
        b->estable = b->candidato = pin_leer(b->pin) == b->activo_alto;
        b->t_candidato = ahora;
    }
}

void botones_actualizar(void)
{
    uint32_t ahora = tiempo_us();

    for (int i = 0; i < BOTON_CANTIDAD; i++) {
        boton_t *b = &botones[i];
        bool lectura = pin_leer(b->pin) == b->activo_alto;

        if (lectura != b->candidato) {
            b->candidato = lectura;
            b->t_candidato = ahora;
        } else if (lectura != b->estable &&
                   (uint32_t)(ahora - b->t_candidato) >= BOTON_ANTIREBOTE_US) {
            b->estable = lectura;
            if (lectura) {
                b->ev_pulsado = true;
                b->t_pulsado = b->t_candidato;
                b->largo_avisado = false;
            } else {
                b->ev_soltado = true;
                b->duracion_us = b->t_candidato - b->t_pulsado;
            }
        }

        if (b->estable && !b->largo_avisado &&
            (uint32_t)(ahora - b->t_pulsado) >= BOTON_LARGO_US) {
            b->largo_avisado = true;
            b->ev_largo = true;
        }
    }
}

bool boton_presionado(boton_id_t b)
{
    return botones[b].estable;
}

static bool consumir(bool *evento)
{
    bool v = *evento;
    *evento = false;
    return v;
}

bool boton_pulsado(boton_id_t b) { return consumir(&botones[b].ev_pulsado); }
bool boton_soltado(boton_id_t b) { return consumir(&botones[b].ev_soltado); }
bool boton_largo(boton_id_t b)   { return consumir(&botones[b].ev_largo);   }

uint32_t boton_ultima_duracion_ms(boton_id_t b)
{
    return botones[b].duracion_us / 1000u;
}
