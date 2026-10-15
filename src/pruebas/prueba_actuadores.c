/*
 * prueba_actuadores.c — Etapa 4b del plan: buzzer pasivo y LED RGB.
 *
 * Fase 1 (automática, ~6 s):
 *   - LED: rojo, verde, azul, blanco, cian, magenta, amarillo, apagado.
 *     Comprobar que cada color es el correcto (si salen intercambiados o
 *     "invertidos" → polaridad / orden de los pines) y que el blanco se ve
 *     blanco (si no, bajar la escala del canal dominante en actuadores.h).
 *   - Buzzer: escala de Do mayor, de C5 a C6.
 *   - Rampa de brillo del blanco: debe subir de forma uniforme (gamma).
 *
 * Fase 2 (interactiva):
 *   B1 = confirmar (LED verde)   B2 = alerta (LED amarillo)
 *   B3 = error (LED rojo)        B4 = inicio (LED azul)
 *   Joystick: X cambia el tono del LED, Y el brillo; SW apaga/enciende.
 *   El LED vuelve al color del joystick cuando termina el sonido.
 *
 * Con el buzzer sonando, el programa no se bloquea: el joystick y los
 * botones siguen respondiendo (buzzer_actualizar() avanza la secuencia).
 */
#include <stdbool.h>
#include <stdio.h>

#include "pruebas/pruebas.h"
#include "dominio/color.h"
#include "dominio/sonidos.h"
#include "drivers/adc.h"
#include "drivers/botones.h"
#include "drivers/buzzer.h"
#include "drivers/joystick.h"
#include "drivers/led.h"
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
    static const struct { uint8_t r, g, b; const char *nombre; } colores[] = {
        { 255, 0, 0, "rojo" },       { 0, 255, 0, "verde" },
        { 0, 0, 255, "azul" },       { 255, 255, 255, "blanco" },
        { 0, 255, 255, "cian" },     { 255, 0, 255, "magenta" },
        { 255, 255, 0, "amarillo" }, { 0, 0, 0, "apagado" },
    };
    static const uint16_t escala[] = { 523, 587, 659, 698, 784, 880, 988, 1047 };

    printf("LED y notas:\n");
    for (unsigned i = 0; i < sizeof colores / sizeof colores[0]; i++) {
        printf("  %-8s %4u Hz\n", colores[i].nombre, escala[i]);
        led_rgb(colores[i].r, colores[i].g, colores[i].b);
        buzzer_tono(escala[i], 300);
        esperar(450);
    }

    printf("Rampa de brillo del blanco\n");
    for (int v = 0; v <= 255; v += 3) {
        led_rgb((uint8_t)v, (uint8_t)v, (uint8_t)v);
        esperar(12);
    }
    for (int v = 255; v >= 0; v -= 3) {
        led_rgb((uint8_t)v, (uint8_t)v, (uint8_t)v);
        esperar(12);
    }
    led_apagar();
}

void prueba_actuadores(void)
{
    adc_iniciar();
    botones_iniciar();
    joystick_iniciar();
    buzzer_iniciar();
    led_iniciar();

    printf("Actuadores listos.\n");
    buzzer_reproducir(&SONIDO_INICIO);
    esperar(800);

    fase_automatica();

    printf("Modo interactivo: B1 confirmar, B2 alerta, B3 error, B4 inicio, "
           "joystick = color (X) y brillo (Y), SW = apagar/encender\n");

    static const struct { const sonido_t *sonido; color_rgb_t color; const char *nombre; } eventos[] = {
        { &SONIDO_CONFIRMAR, { 0, 255, 0 },   "confirmar" },
        { &SONIDO_ALERTA,    { 255, 200, 0 }, "alerta"    },
        { &SONIDO_ERROR,     { 255, 0, 0 },   "error"     },
        { &SONIDO_INICIO,    { 0, 0, 255 },   "inicio"    },
    };

    int tono = 0;               /* 0..359 */
    int brillo = 128;           /* 0..255 */
    bool encendido = true;
    uint32_t t_joy = 0;
    bool evento_activo = false;

    while (true) {
        botones_actualizar();
        buzzer_actualizar();

        for (int i = 0; i < 4; i++) {
            if (boton_pulsado((boton_id_t)i)) {
                printf("B%d -> %s\n", i + 1, eventos[i].nombre);
                buzzer_reproducir(eventos[i].sonido);
                led_color(eventos[i].color);
                evento_activo = true;
            }
        }
        if (boton_pulsado(BOTON_JOY)) {
            encendido = !encendido;
            printf("LED %s\n", encendido ? "encendido" : "apagado");
        }

        if (evento_activo && !buzzer_ocupado()) {
            evento_activo = false;
        }

        if ((uint32_t)(tiempo_us() - t_joy) >= 20000u) {
            t_joy = tiempo_us();
            joystick_t j;
            joystick_leer(&j);

            tono = (tono + j.x / 10 + 360) % 360;
            brillo += j.y / 10;
            if (brillo < 0)   { brillo = 0; }
            if (brillo > 255) { brillo = 255; }

            if (!evento_activo) {
                if (encendido) {
                    color_hsv_t hsv = { (int16_t)tono, 255, (uint8_t)brillo };
                    led_color(color_desde_hsv(hsv));
                } else {
                    led_apagar();
                }
            }
        }
    }
}
