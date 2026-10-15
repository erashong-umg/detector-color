/*
 * prueba_entradas.c — Etapa 4 del plan: botones y joystick.
 *
 * Pantalla (ST7789):
 *   - Fila superior, de izquierda a derecha: B1, B2, B3, B4 y SW del joystick.
 *     Gris = suelto, verde = pulsado, magenta = pulsación larga (≥ 800 ms).
 *   - Recuadro central: posición del joystick (punto blanco) sobre una cruz;
 *     el punto se pone rojo mientras el joystick está fuera de la zona muerta.
 *
 * USB (115200): un mensaje por evento ("B2 pulsado", "B2 largo",
 * "B2 soltado (350 ms)") y, cada vez que el joystick se mueve ≥ 3 puntos,
 * sus valores normalizados y crudos. Al arrancar imprime el centro medido.
 *
 * Qué comprobar:
 *   - Una pulsación limpia genera exactamente un "pulsado" y un "soltado"
 *     (si salen varios, el antirrebote es corto → entradas.h).
 *   - Joystick en reposo: x = 0, y = 0 y el punto no se mueve. Si el reposo
 *     da valores ≠ 0, la zona muerta es pequeña para este potenciómetro.
 *   - Extremos: ±100 en cada dirección; arriba debe dar y positivo.
 */
#include <stdbool.h>
#include <stdio.h>

#include "pruebas/pruebas.h"
#include "configuracion/entradas.h"
#include "configuracion/pantalla.h"
#include "drivers/adc.h"
#include "drivers/botones.h"
#include "drivers/joystick.h"
#include "drivers/st7789.h"
#include "hw/tiempo.h"

#define GRIS         ST7789_RGB565(60, 60, 60)
#define GRIS_CLARO   ST7789_RGB565(110, 110, 110)

/* Caja del joystick */
#define CAJA_X       55
#define CAJA_Y       70
#define CAJA_LADO    130
#define CAJA_MEDIO   (CAJA_LADO / 2)
#define PUNTO        10

static const char *nombres[BOTON_CANTIDAD] = { "B1", "B2", "B3", "B4", "SW" };

static void dibujar_boton(int i, uint16_t color)
{
    st7789_rect(4 + i * 48, 10, 40, 36, color);
}

static void dibujar_caja(void)
{
    st7789_rect(CAJA_X - 2, CAJA_Y - 2, CAJA_LADO + 4, CAJA_LADO + 4, GRIS_CLARO);
    st7789_rect(CAJA_X, CAJA_Y, CAJA_LADO, CAJA_LADO, ST7789_NEGRO);
}

static void dibujar_joystick(int x, int y, bool activo, int *px, int *py)
{
    /* Borra el punto anterior y repone la cruz */
    st7789_rect(*px - PUNTO / 2, *py - PUNTO / 2, PUNTO, PUNTO, ST7789_NEGRO);
    st7789_rect(CAJA_X, CAJA_Y + CAJA_MEDIO, CAJA_LADO, 1, GRIS);
    st7789_rect(CAJA_X + CAJA_MEDIO, CAJA_Y, 1, CAJA_LADO, GRIS);

    /* ±100 → ±(CAJA_MEDIO − PUNTO/2) */
    int alcance = CAJA_MEDIO - PUNTO / 2 - 1;
    *px = CAJA_X + CAJA_MEDIO + x * alcance / 100;
    *py = CAJA_Y + CAJA_MEDIO - y * alcance / 100;     /* arriba = y positivo */
    st7789_rect(*px - PUNTO / 2, *py - PUNTO / 2, PUNTO, PUNTO,
                activo ? ST7789_ROJO : ST7789_BLANCO);
}

static int abs_i(int v) { return v < 0 ? -v : v; }

void prueba_entradas(void)
{
    st7789_iniciar();
    adc_iniciar();
    botones_iniciar();
    joystick_iniciar();     /* mide el centro: joystick quieto al arrancar */

    uint16_t cx, cy;
    joystick_centro(&cx, &cy);
    printf("Entradas listas. Centro del joystick: X=%u Y=%u (ideal ~2048)\n", cx, cy);

    for (int i = 0; i < BOTON_CANTIDAD; i++) {
        dibujar_boton(i, GRIS);
    }
    dibujar_caja();

    int px = CAJA_X + CAJA_MEDIO, py = CAJA_Y + CAJA_MEDIO;
    int ult_x = 1000, ult_y = 1000;         /* fuerza el primer dibujo */
    int imp_x = 1000, imp_y = 1000;
    uint32_t t_joy = 0;

    while (true) {
        botones_actualizar();

        for (int i = 0; i < BOTON_CANTIDAD; i++) {
            boton_id_t b = (boton_id_t)i;
            if (boton_pulsado(b)) {
                printf("%s pulsado\n", nombres[i]);
                dibujar_boton(i, ST7789_VERDE);
            }
            if (boton_largo(b)) {
                printf("%s largo\n", nombres[i]);
                dibujar_boton(i, ST7789_MAGENTA);
            }
            if (boton_soltado(b)) {
                printf("%s soltado (%lu ms)\n", nombres[i],
                       (unsigned long)boton_ultima_duracion_ms(b));
                dibujar_boton(i, GRIS);
            }
        }

        /* El joystick se lee cada 20 ms: cada lectura son 8 conversiones. */
        if ((uint32_t)(tiempo_us() - t_joy) >= 20000u) {
            t_joy = tiempo_us();
            joystick_t j;
            joystick_leer(&j);

            if (j.x != ult_x || j.y != ult_y) {
                ult_x = j.x;
                ult_y = j.y;
                dibujar_joystick(j.x, j.y, j.x != 0 || j.y != 0, &px, &py);
            }
            if (abs_i(j.x - imp_x) >= 3 || abs_i(j.y - imp_y) >= 3) {
                imp_x = j.x;
                imp_y = j.y;
                printf("JOY x=%4d y=%4d  (crudo %4u, %4u)\n", j.x, j.y, j.crudo_x, j.crudo_y);
            }
        }

        tiempo_esperar_ms(1);
    }
}
