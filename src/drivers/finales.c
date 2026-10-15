#include "drivers/finales.h"

#include <stdint.h>
#include "configuracion/motor.h"
#include "configuracion/pines.h"
#include "hw/pin.h"
#include "hw/tiempo.h"

typedef struct {
    uint8_t  pin;
    bool     estable;       /* true = extremo alcanzado */
    bool     candidato;
    uint32_t t_candidato;
} final_t;

static final_t inicio = { .pin = PIN_IR_INICIO };
static final_t fin    = { .pin = PIN_IR_FIN };

static bool leer_activo(const final_t *f)
{
    bool nivel_alto = pin_leer(f->pin);
    return FINAL_ACTIVO_BAJO ? !nivel_alto : nivel_alto;
}

static void iniciar_uno(final_t *f)
{
    /* Pull hacia el estado activo: bajo si es activo en bajo, alto si no. */
    pin_entrada(f->pin, FINAL_ACTIVO_BAJO ? PIN_PULL_DOWN : PIN_PULL_UP);
    f->estable = f->candidato = leer_activo(f);
    f->t_candidato = tiempo_us();
}

static void actualizar_uno(final_t *f, uint32_t ahora)
{
    bool lectura = leer_activo(f);

    if (lectura != f->candidato) {
        f->candidato = lectura;
        f->t_candidato = ahora;
    } else if (lectura != f->estable &&
               (uint32_t)(ahora - f->t_candidato) >= FINAL_ANTIREBOTE_MS * 1000u) {
        f->estable = lectura;
    }
}

void finales_iniciar(void)
{
    iniciar_uno(&inicio);
    iniciar_uno(&fin);
    /* El pull-down (o up) tarda unos µs en asentar el nivel con un cable suelto. */
    tiempo_esperar_ms(FINAL_ANTIREBOTE_MS + 2u);
    finales_actualizar();
    inicio.estable = leer_activo(&inicio);
    fin.estable    = leer_activo(&fin);
}

void finales_actualizar(void)
{
    uint32_t ahora = tiempo_us();
    actualizar_uno(&inicio, ahora);
    actualizar_uno(&fin, ahora);
}

bool final_inicio_activo(void) { return inicio.estable; }
bool final_fin_activo(void)    { return fin.estable;    }
bool final_inicio_crudo(void)  { return pin_leer(inicio.pin); }
bool final_fin_crudo(void)     { return pin_leer(fin.pin);    }
