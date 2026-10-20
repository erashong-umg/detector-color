/*
 * prueba_actuadores.c — Etapa 4b del plan: buzzer pasivo.
 *
 * Fase 1 (automática, ~4 s):
 *   - Escala de Do mayor, de C5 a C6.
 *
 * Fase 2 (interactiva):
 *   B1 = confirmar   B2 = alerta   B3 = error   B4 = inicio
 *   SW del joystick = siguiente sonido de la lista: clic, moviendo, llegada, home, analizando
 *   Cada botón reproduce su secuencia de sonidos.
 *
 * Con el buzzer sonando, el programa no se bloquea: los botones siguen
 * respondiendo (buzzer_actualizar() avanza la secuencia).
 */
#include <stdbool.h>
#include <stdio.h>

#include "pruebas/pruebas.h"
#include "dominio/sonidos.h"
#include "drivers/botones.h"
#include "drivers/buzzer.h"
#include "hw/tiempo.h"

/* Espera ms milisegundos manteniendo vivo el secuenciador del buzzer. */
static void esperar(uint32_t ms)
{
    uint32_t t0 = tiempo_us();
    while ((uint32_t)(tiempo_us() - t0) < ms * 1000u) {
        buzzer_actualizar();
    }
}

static void fase_automatica(void)
{
    static const uint16_t escala[] = { 523, 587, 659, 698, 784, 880, 988, 1047 };

    printf("Notas:\n");
    for (unsigned i = 0; i < sizeof escala / sizeof escala[0]; i++) {
        printf("  %4u Hz\n", escala[i]);
        buzzer_tono(escala[i], 300);
        esperar(450);
    }
}

void prueba_actuadores(void)
{
    botones_iniciar();
    buzzer_iniciar();

    printf("Buzzer listo.\n");
    buzzer_reproducir(&SONIDO_INICIO);
    esperar(800);

    fase_automatica();

    printf("Modo interactivo: B1 confirmar, B2 alerta, B3 error, B4 inicio\n");

    static const struct { const sonido_t *sonido; const char *nombre; } eventos[] = {
        { &SONIDO_CONFIRMAR, "confirmar" },
        { &SONIDO_ALERTA,    "alerta"    },
        { &SONIDO_ERROR,     "error"     },
        { &SONIDO_INICIO,    "inicio"    },
    };

    static const struct { const sonido_t *sonido; const char *nombre; } extras[] = {
        { &SONIDO_CLIC,       "clic"       },
        { &SONIDO_MOVIENDO,   "moviendo"   },
        { &SONIDO_LLEGADA,    "llegada"    },
        { &SONIDO_HOME,       "home"       },
        { &SONIDO_ANALIZANDO, "analizando" },
    };
    unsigned i_extra = 0;

    while (true) {
        botones_actualizar();
        buzzer_actualizar();

        if (boton_pulsado(BOTON_JOY)) {
            printf("SW -> %s\n", extras[i_extra].nombre);
            buzzer_reproducir(extras[i_extra].sonido);
            i_extra = (i_extra + 1) % (sizeof extras / sizeof extras[0]);
        }

        for (int i = 0; i < 4; i++) {
            if (boton_pulsado((boton_id_t)i)) {
                printf("B%d -> %s\n", i + 1, eventos[i].nombre);
                buzzer_reproducir(eventos[i].sonido);
            }
        }
    }
}
