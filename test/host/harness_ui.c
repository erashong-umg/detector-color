/* Arnés: ejecuta el código REAL de la interfaz contra la pantalla simulada. */
#include <stdio.h>
#include <string.h>
#include "ui/interfaz.h"
#include "dominio/color.h"
#include "dominio/historial.h"
#include "drivers/st7789.h"

uint32_t mock_us;
extern int fuera_de_pantalla;
void volcar_ppm(const char *ruta);

static ui_modelo_t m;
static historial_t hist;
static const char *dir_salida;

static void pintar(const char *nombre)
{
    for (int i = 0; i < 40; i++) ui_dibujar(&m);    /* de sobra para todas las secciones */
    char ruta[512];
    snprintf(ruta, sizeof ruta, "%s/%s.ppm", dir_salida, nombre);
    volcar_ppm(ruta);
}

static void tecla(ui_entrada_t e) { (void)ui_procesar(&e, &m); }

static void lectura(uint8_t r, uint8_t g, uint8_t b, int32_t objetivo)
{
    m.hay_lectura = true;
    m.rgb.r = r; m.rgb.g = g; m.rgb.b = b;
    color_a_hex(m.rgb, m.hex);
    color_hsv_t h = color_a_hsv(m.rgb);
    m.h = h.h; m.s = h.s; m.v = h.v;
    m.objetivo_um = objetivo;
}

int main(int argc, char **argv)
{
    dir_salida = argc > 1 ? argv[1] : ".";
    historial_iniciar(&hist);
    m.historial = &hist;
    m.objetivo_um = -1; m.pos_um = -1;
    snprintf(m.estado, sizeof m.estado, "DESARMADO"); m.estado_color = ST7789_RGB565(135, 140, 152);
    snprintf(m.eje, sizeof m.eje, "SIN_REFERENCIA");
    snprintf(m.prox, sizeof m.prox, "nada");
    ui_iniciar();

    /* 1. Principal, sin lectura, desarmado */
    snprintf(m.mensaje, sizeof m.mensaje, "Calibra negro y blanco para empezar");
    pintar("01_principal_vacia");

    /* 2. Principal con un color, armado y referenciado, cursor en camino */
    lectura(212, 48, 35, 6800);
    snprintf(m.estado, sizeof m.estado, "MOVIENDO"); m.estado_color = ST7789_RGB565(80, 155, 245);
    snprintf(m.eje, sizeof m.eje, "MOVIENDO");
    m.chip_cal = m.chip_ref = m.chip_arm = true;
    m.pos_um = 123400; m.objetivo_um = 261400;
    m.clase = ESCALA_CROMATICO; m.temperatura = TEMP_CALIDO;
    snprintf(m.mensaje, sizeof m.mensaje, "Moviendo el cursor");
    m.ir_inicio = false; m.ir_fin = false;
    snprintf(m.prox, sizeof m.prox, "ok");
    m.h = 7;
    pintar("02_principal_color");

    /* 3. Color claro (texto negro) */
    lectura(240, 225, 60, 55400);
    pintar("03_principal_amarillo");

    /* 4. Blanco: acromático */
    lectura(250, 248, 245, -1); m.clase = ESCALA_BLANCO; m.temperatura = TEMP_NEUTRO; m.h = -1;
    snprintf(m.estado, sizeof m.estado, "LISTO"); m.estado_color = ST7789_RGB565(70, 200, 120);
    snprintf(m.mensaje, sizeof m.mensaje, "Medido: neutro");
    m.pos_um = 350000; m.ir_fin = true;
    snprintf(m.prox, sizeof m.prox, "cerca");
    pintar("04_principal_blanco");

    /* 5. Error */
    snprintf(m.estado, sizeof m.estado, "ERROR"); m.estado_color = ST7789_RGB565(235, 75, 75);
    snprintf(m.error, sizeof m.error, "Los dos finales activos a la vez");
    m.ir_inicio = m.ir_fin = true;
    pintar("05_principal_error");
    m.error[0] = 0; m.ir_inicio = m.ir_fin = false;

    /* 6. Menú */
    { ui_entrada_t e = { 0 }; e.b4 = true; tecla(e); }
    { ui_entrada_t e = { 0 }; e.b3 = true; tecla(e); tecla(e); tecla(e);      /* B3 = abajo, como el joystick */ }
    { ui_entrada_t e = { 0 }; e.b2 = true; tecla(e); }                          /* B2 = arriba */
    pintar("06_menu");

    /* 7. Historial con entradas */
    for (int i = 0; i < 9; i++) {
        entrada_hist_t h;
        color_rgb_t c = { (uint8_t)(40 + i * 25), (uint8_t)(220 - i * 20), (uint8_t)(60 + i * 18) };
        h.rgb = c; h.h = 100; h.x_um = (i % 4 == 3) ? -1 : 30000 + i * 31000;
        historial_agregar(&hist, &h);
    }
    { ui_entrada_t e = { 0 }; e.dir = +1; tecla(e); tecla(e); tecla(e); }   /* menú: 3 -> 6 = Historial */
    { ui_entrada_t e = { 0 }; e.b1 = true; tecla(e); }              /* B1 = SW: menú -> historial */
    { ui_entrada_t e = { 0 }; e.dir = +1; tecla(e); tecla(e); tecla(e); }
    pintar("07_historial");

    /* 8. Historial vacío */
    {
        historial_t vacio; historial_iniciar(&vacio);
        const historial_t *guardado = m.historial; m.historial = &vacio;
        ui_iniciar();
        { ui_entrada_t e = { 0 }; e.b4 = true; tecla(e); }               /* principal -> menú */
        { ui_entrada_t e = { 0 }; e.dir = +1; for (int i = 0; i < 6; i++) tecla(e); }
        { ui_entrada_t e = { 0 }; e.b1 = true; tecla(e); }              /* menú -> historial */
        pintar("08_historial_vacio");
        m.historial = guardado;
    }

    /* 9. Mover cursor */
    { ui_entrada_t e = { 0 }; e.b4 = true; tecla(e); }              /* vuelve a principal */
    m.pos_um = 123400;
    { ui_entrada_t e = { 0 }; e.b3 = true; tecla(e); }              /* B3 en la principal no hace nada */
    if (ui_pantalla() != UI_PANTALLA_PRINCIPAL) {
        printf("FALLO: B3 en la pantalla principal no debe cambiar de pantalla\n");
        return 1;
    }
    { ui_entrada_t e = { 0 }; e.b4 = true; tecla(e); }               /* principal -> menú */
    { ui_entrada_t e = { 0 }; e.dir = +1; for (int i = 0; i < 4; i++) tecla(e); }   /* 0 -> 4 = Mover cursor */
    { ui_entrada_t e = { 0 }; e.b1 = true; tecla(e); }              /* menú -> mover */
    { ui_entrada_t e = { 0 }; e.joy_x = 60; for (int i = 0; i < 30; i++) tecla(e); }
    pintar("09_mover");

    /* 10. Información */
    { ui_entrada_t e = { 0 }; e.b4 = true; tecla(e); }
    { ui_entrada_t e = { 0 }; e.b4 = true; tecla(e); }               /* principal -> menú */
    { ui_entrada_t e = { 0 }; e.dir = -1; tecla(e); }               /* 0 -> 7 = Información */
    { ui_entrada_t e = { 0 }; e.b1 = true; tecla(e); }
    m.hay_negro = true; m.hay_blanco = false; m.recorrido_ms = 782;
    snprintf(m.eje, sizeof m.eje, "LISTO");
    pintar("10_info");

    /* 11-14. Asistente de inicio: eje, negro, blanco y listo */
    ui_iniciar();
    ui_abrir_asistente();
    m.chip_ref = false; m.hay_negro = false; m.hay_blanco = false; m.midiendo = false;
    snprintf(m.estado, sizeof m.estado, "HOMING"); snprintf(m.mensaje, sizeof m.mensaje, "%s", "");
    pintar("11_asistente_eje");
    m.chip_ref = true;
    snprintf(m.estado, sizeof m.estado, "LISTO");
    pintar("12_asistente_negro");
    m.hay_negro = true; m.midiendo = true;
    snprintf(m.estado, sizeof m.estado, "CAL. COLOR");
    snprintf(m.mensaje, sizeof m.mensaje, "Acerca el objeto");
    pintar("13_asistente_blanco");
    m.hay_blanco = true; m.midiendo = false; m.mensaje[0] = 0;
    snprintf(m.estado, sizeof m.estado, "LISTO");
    pintar("14_asistente_listo");

    printf("Bloques descartados por salirse de la pantalla: %d\n", fuera_de_pantalla);
    return 0;
}
