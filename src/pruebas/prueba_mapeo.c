/*
 * prueba_mapeo.c — Etapa 7 del plan: del color a la posición en la escala.
 *
 * Integra todo: TCS3200, calibración, mapeo (dominio/escala.c) y eje.
 * Funciona también SIN motor ni eje referenciado: entonces solo informa de
 * dónde se movería el cursor (modo simulado).
 *
 * Controles:
 *   B1  medir el color de la muestra y apuntar el cursor
 *   B2  calibrar NEGRO  (muestra negra mate frente al sensor)
 *   B3  calibrar BLANCO (papel blanco a la misma distancia)
 *   B4  referenciar el eje (homing + calibración del recorrido)
 *   B4 largo  reinicia un error del eje
 *   SW  arma / desarma el puente del motor
 * Orden de uso: B2 (negro) → B3 (blanco) → B1 con muestras de color.
 *
 * Pantalla: arriba, el color medido; debajo, la escala de tono de 0° a 360°
 * (la misma que debe estar impresa) con una marca BLANCA sobre ella en el
 * objetivo y una AMARILLA debajo en la posición actual del cursor. Cuadrito
 * en la esquina: verde = colores calibrados, rojo = sin calibrar.
 * El LED RGB reproduce el color medido.
 *
 * Qué comprobar con muestras impresas:
 *   - que el cursor llegue a la zona de la escala impresa de ese color;
 *   - la estabilidad: medir 10 veces la misma muestra y ver cuánto varía H
 *     (1° = 1 mm). Si varía más que ESCALA_ZONA_MUERTA_UM, subir ese valor;
 *   - blanco, gris y negro NO mueven el cursor (política acromática).
 */
#include <stdbool.h>
#include <stdio.h>

#include "pruebas/pruebas.h"
#include "aplicacion/carro.h"
#include "aplicacion/deteccion.h"
#include "aplicacion/eje.h"
#include "configuracion/escala.h"
#include "configuracion/pantalla.h"
#include "dominio/color.h"
#include "dominio/sonidos.h"
#include "drivers/adc.h"
#include "drivers/botones.h"
#include "drivers/buzzer.h"
#include "drivers/joystick.h"
#include "drivers/led.h"
#include "drivers/st7789.h"
#include "hw/tiempo.h"

#define MUESTRA_ALTO   120
#define ESCALA_Y       150
#define ESCALA_ALTO    36
#define MARCA_OBJ_Y    (ESCALA_Y - 16)
#define MARCA_POS_Y    (ESCALA_Y + ESCALA_ALTO + 2)
#define MARCA_ALTO     14
#define GRIS_OSCURO    ST7789_RGB565(40, 40, 40)

static int um_a_px(int32_t um)
{
    int px = (int)((int64_t)(um - ESCALA_DESPLAZAMIENTO_UM) * TFT_ANCHO / ESCALA_ANCHO_UM);
    if (px < 0) { px = 0; }
    if (px > TFT_ANCHO - 1) { px = TFT_ANCHO - 1; }
    return px;
}

static void dibujar_escala(void)
{
    static uint16_t columna[ESCALA_ALTO];
    for (int x = 0; x < TFT_ANCHO; x++) {
        /* El tono impreso en cada columna sigue la misma fórmula que el mapeo. */
        int h = (ESCALA_TONO_INICIAL_GRADOS + x * 360 / TFT_ANCHO) % 360;
        color_hsv_t hsv = { (int16_t)h, 255, 255 };
        color_rgb_t c = color_desde_hsv(hsv);
        for (int y = 0; y < ESCALA_ALTO; y++) {
            columna[y] = ST7789_RGB565(c.r, c.g, c.b);
        }
        st7789_bloque(x, ESCALA_Y, 1, ESCALA_ALTO, columna);
    }
}

static void dibujar_marca(int *px_anterior, int32_t um, int y, uint16_t color)
{
    int px = um < 0 ? -1 : um_a_px(um);
    if (px == *px_anterior) {
        return;
    }
    if (*px_anterior >= 0) {
        st7789_rect(*px_anterior - 2, y, 5, MARCA_ALTO, ST7789_NEGRO);
    }
    if (px >= 0) {
        st7789_rect(px - 1, y, 3, MARCA_ALTO, color);
    }
    *px_anterior = px;
}

static void dibujar_muestra(const lectura_t *l)
{
    uint16_t c = l->valida ? ST7789_RGB565(l->rgb.r, l->rgb.g, l->rgb.b) : GRIS_OSCURO;
    st7789_rect(0, 0, TFT_ANCHO, MUESTRA_ALTO, c);
}

static void dibujar_calibracion(void)
{
    st7789_rect(TFT_ANCHO - 20, 4, 16, 16,
                deteccion_calibrado() ? ST7789_VERDE : ST7789_ROJO);
}

static void imprimir_mm(const char *pre, int32_t um)
{
    printf("%s%ld.%01ld mm", pre, (long)(um / 1000), (long)((um % 1000) / 100));
}

static void mostrar_lectura(const lectura_t *l, det_resultado_t r, bool simulado)
{
    printf("Hz R=%lu G=%lu B=%lu C=%lu | RGB=%u,%u,%u %s | H=%d S=%u V=%u | %s",
           (unsigned long)l->f.r, (unsigned long)l->f.g, (unsigned long)l->f.b,
           (unsigned long)l->f.c, l->rgb.r, l->rgb.g, l->rgb.b, l->hex,
           l->hsv.h, l->hsv.s, l->hsv.v, escala_clase_texto(l->escala.clase));
    if (l->escala.tiene_posicion) {
        imprimir_mm(" | objetivo ", l->escala.x_um);
    }
    printf(" -> %s%s\n", deteccion_resultado_texto(r), simulado ? " [simulado]" : "");
}

void prueba_mapeo(void)
{
    adc_iniciar();
    botones_iniciar();
    joystick_iniciar();
    buzzer_iniciar();
    led_iniciar();
    st7789_iniciar();
    carro_iniciar();
    eje_iniciar();
    deteccion_iniciar();

    st7789_rellenar(ST7789_NEGRO);
    dibujar_escala();
    dibujar_calibracion();

    lectura_t lectura = { 0 };
    dibujar_muestra(&lectura);
    int marca_obj = -1, marca_pos = -1;
    int32_t objetivo_um = -1;

    printf("\n== Prueba color -> posicion ==\n");
    printf("B2 calibrar negro, B3 calibrar blanco, B1 medir y apuntar, B4 referenciar, SW armar\n");
    if (carro_error() != CARRO_ERROR_NINGUNO) {
        printf("ERROR: %s\n", carro_error_texto(carro_error()));
        buzzer_reproducir(&SONIDO_ERROR);
    } else {
        buzzer_reproducir(&SONIDO_INICIO);
    }

    eje_estado_t ult_estado = eje_estado();
    uint32_t t_marca = 0;

    while (true) {
        botones_actualizar();
        buzzer_actualizar();
        eje_actualizar();

        /* --- Eventos del eje --- */
        if (eje_estado() != ult_estado) {
            printf("eje: %s -> %s\n", eje_estado_texto(ult_estado), eje_estado_texto(eje_estado()));
            if (eje_estado() == EJE_ERROR) {
                printf("ERROR: %s\n", eje_error_texto(eje_error()));
                buzzer_reproducir(&SONIDO_ERROR);
            } else if (eje_estado() == EJE_LISTO) {
                imprimir_mm("cursor en ", eje_posicion_um());
                printf("\n");
                buzzer_reproducir(&SONIDO_CONFIRMAR);
            }
            ult_estado = eje_estado();
        }

        /* --- Botones --- */
        if (boton_pulsado(BOTON_JOY)) {
            bool armar = !carro_armado();
            if (carro_armar(armar)) {
                printf("Puente %s\n", armar ? "ARMADO" : "desarmado");
                buzzer_reproducir(armar ? &SONIDO_CONFIRMAR : &SONIDO_CLIC);
                if (!armar) { eje_parar(); }
            } else {
                printf("No se puede armar: %s\n", carro_error_texto(carro_error()));
                buzzer_reproducir(&SONIDO_ERROR);
            }
        }
        if (boton_pulsado(BOTON_2)) {
            printf("Calibrando NEGRO...\n");
            if (deteccion_calibrar_negro()) {
                printf("negro calibrado%s\n", deteccion_calibrado() ? " (listo)" : "");
                buzzer_reproducir(&SONIDO_CONFIRMAR);
            } else {
                printf("No se pudo calibrar (desborde o carrito en movimiento)\n");
                buzzer_reproducir(&SONIDO_ERROR);
            }
            dibujar_calibracion();
        }
        if (boton_pulsado(BOTON_3)) {
            printf("Calibrando BLANCO...\n");
            if (deteccion_calibrar_blanco()) {
                printf("blanco calibrado%s\n", deteccion_calibrado() ? " (listo)" : "");
                buzzer_reproducir(&SONIDO_CONFIRMAR);
            } else {
                printf("No se pudo calibrar (desborde o carrito en movimiento)\n");
                buzzer_reproducir(&SONIDO_ERROR);
            }
            dibujar_calibracion();
        }
        if (boton_pulsado(BOTON_4)) {
            if (eje_referenciar()) {
                printf("Referenciando el eje...\n");
            } else {
                printf("No se puede referenciar: armar (SW) o reiniciar el error\n");
                buzzer_reproducir(&SONIDO_ALERTA);
            }
        }
        if (boton_largo(BOTON_4)) {
            if (eje_reiniciar()) {
                printf("Error reiniciado: referenciar de nuevo (B4)\n");
                buzzer_reproducir(&SONIDO_CONFIRMAR);
            } else {
                printf("El error persiste: %s\n", carro_error_texto(carro_error()));
                buzzer_reproducir(&SONIDO_ERROR);
            }
        }

        /* --- Medir y apuntar --- */
        if (boton_pulsado(BOTON_1)) {
            if (deteccion_medir(&lectura)) {
                det_resultado_t r = deteccion_apuntar(&lectura);
                bool simulado = (r == DET_SIN_REFERENCIA || r == DET_NO_PUEDE);
                mostrar_lectura(&lectura, r, simulado);
                led_color(lectura.rgb);
                objetivo_um = lectura.escala.tiene_posicion ? lectura.escala.x_um : -1;
                buzzer_reproducir(r == DET_ACROMATICO ? &SONIDO_ALERTA : &SONIDO_CONFIRMAR);
            } else if (!deteccion_calibrado()) {
                printf("Falta calibrar: B2 (negro) y B3 (blanco)\n");
                buzzer_reproducir(&SONIDO_ALERTA);
            } else {
                printf("Lectura no valida (desborde del contador o carrito en movimiento)\n");
                buzzer_reproducir(&SONIDO_ERROR);
            }
            dibujar_muestra(&lectura);
            dibujar_marca(&marca_obj, objetivo_um, MARCA_OBJ_Y, ST7789_BLANCO);
        }

        /* --- Marca de posición actual (cada 100 ms) --- */
        if ((uint32_t)(tiempo_us() - t_marca) >= 100000u) {
            t_marca = tiempo_us();
            dibujar_marca(&marca_pos, eje_posicion_um(), MARCA_POS_Y, ST7789_AMARILLO);
        }

        tiempo_esperar_us(500);
    }
}
