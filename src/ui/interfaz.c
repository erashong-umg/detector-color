#include "ui/interfaz.h"

#include <stdio.h>

#include "configuracion/eje.h"
#include "configuracion/escala.h"
#include "configuracion/pantalla.h"
#include "drivers/st7789.h"
#include "graficos/texto.h"

/* ---- Paleta ---- */
#define C_FONDO    ST7789_RGB565(14, 16, 22)
#define C_PANEL    ST7789_RGB565(32, 36, 48)
#define C_CHIP     ST7789_RGB565(55, 60, 75)
#define C_TEXTO    ST7789_RGB565(235, 235, 240)
#define C_TENUE    ST7789_RGB565(135, 140, 152)
#define C_OK       ST7789_RGB565(70, 200, 120)
#define C_AVISO    ST7789_RGB565(240, 175, 50)
#define C_ERROR    ST7789_RGB565(235, 75, 75)
#define C_ACENTO   ST7789_RGB565(80, 155, 245)
#define C_SEL      ST7789_RGB565(45, 85, 160)
#define C_R        ST7789_RGB565(245, 95, 95)
#define C_G        ST7789_RGB565(95, 225, 125)
#define C_B        ST7789_RGB565(105, 165, 255)

/* ---- Geometría común ---- */
#define MARGEN        6
#define ANCHO_UTIL    (TFT_ANCHO - 2 * MARGEN)       /* 228 */
#define BARRA_ALTO    22
#define FILA_MENU     20

/* ---- Estado de la interfaz ---- */
typedef enum {
    M_MEDIR, M_CAL_NEGRO, M_CAL_BLANCO, M_REFERENCIAR, M_MOVER, M_MANUAL, M_HISTORIAL, M_ARMAR, M_INFO,
    M_CANTIDAD
} item_menu_t;

static ui_pantalla_t pantalla;
static bool          pantalla_nueva;     /* hay que borrar y pintar lo estático */
static unsigned      seccion;            /* sección que toca pintar             */
static int           menu_sel;
static int           hist_sel;
static int32_t       mover_um;
static int           marca_obj_px = -1;
static int           marca_pos_px = -1;

void ui_iniciar(void)
{
    pantalla = UI_PANTALLA_PRINCIPAL;
    pantalla_nueva = true;
    seccion = 0;
    menu_sel = 0;
    hist_sel = 0;
    mover_um = 0;
}

ui_pantalla_t ui_pantalla(void)
{
    return pantalla;
}

/* ================================================================== */
/* Navegación                                                          */
/* ================================================================== */

static void ir_a_pantalla(ui_pantalla_t p)
{
    pantalla = p;
    pantalla_nueva = true;
    seccion = 0;
}

static void entrar_mover(const ui_modelo_t *m)
{
    mover_um = m->pos_um >= 0 ? m->pos_um : 0;
    ir_a_pantalla(UI_PANTALLA_MOVER);
}

static ui_orden_t orden(ui_accion_t a, int32_t um)
{
    ui_orden_t o = { a, um };
    return o;
}

static ui_orden_t ejecutar_item(item_menu_t item, const ui_modelo_t *m)
{
    switch (item) {
    case M_MEDIR:       ir_a_pantalla(UI_PANTALLA_PRINCIPAL); return orden(UI_ACC_MEDIR, 0);
    case M_CAL_NEGRO:   ir_a_pantalla(UI_PANTALLA_PRINCIPAL); return orden(UI_ACC_CAL_NEGRO, 0);
    case M_CAL_BLANCO:  ir_a_pantalla(UI_PANTALLA_PRINCIPAL); return orden(UI_ACC_CAL_BLANCO, 0);
    case M_REFERENCIAR: ir_a_pantalla(UI_PANTALLA_PRINCIPAL); return orden(UI_ACC_REFERENCIAR, 0);
    case M_MOVER:       entrar_mover(m); break;
    case M_MANUAL:      ir_a_pantalla(UI_PANTALLA_PRINCIPAL); return orden(UI_ACC_MANUAL, 0);
    case M_HISTORIAL:   hist_sel = 0; ir_a_pantalla(UI_PANTALLA_HISTORIAL); break;
    case M_ARMAR:
        ir_a_pantalla(UI_PANTALLA_PRINCIPAL);
        return orden(m->armado ? UI_ACC_DESARMAR : UI_ACC_ARMAR, 0);
    case M_INFO:        ir_a_pantalla(UI_PANTALLA_INFO); break;
    case M_CANTIDAD:    break;
    }
    return orden(UI_ACC_NINGUNA, 0);
}

static int rotar(int valor, int paso, int cantidad)
{
    return (valor + paso + cantidad) % cantidad;
}

ui_orden_t ui_procesar(const ui_entrada_t *e, const ui_modelo_t *m)
{
    switch (pantalla) {
    case UI_PANTALLA_PRINCIPAL:
        if (e->b1) {
            return orden(UI_ACC_MEDIR, 0);
        }
        if (e->b2) {
            menu_sel = M_CAL_NEGRO;
            ir_a_pantalla(UI_PANTALLA_MENU);
        } else if (e->b3) {
            entrar_mover(m);
        } else if (e->b4) {
            return orden(UI_ACC_GUARDAR, 0);
        } else if (e->b4_largo || e->ok) {
            menu_sel = M_MEDIR;
            ir_a_pantalla(UI_PANTALLA_MENU);
        }
        break;

    case UI_PANTALLA_MENU:
        if (e->dir) {
            menu_sel = rotar(menu_sel, e->dir, M_CANTIDAD);
        }
        if (e->b4 || e->b4_largo) {
            ir_a_pantalla(UI_PANTALLA_PRINCIPAL);
        } else if (e->ok) {
            return ejecutar_item((item_menu_t)menu_sel, m);
        }
        break;

    case UI_PANTALLA_HISTORIAL: {
        int n = m->historial ? historial_cantidad(m->historial) : 0;
        if (e->dir && n > 0) {
            hist_sel = rotar(hist_sel, e->dir, n);
        }
        if (e->b4 || e->b4_largo) {
            ir_a_pantalla(UI_PANTALLA_PRINCIPAL);
        } else if (e->ok && n > 0) {
            entrada_hist_t h;
            if (historial_obtener(m->historial, (uint8_t)hist_sel, &h) && h.x_um >= 0) {
                ir_a_pantalla(UI_PANTALLA_PRINCIPAL);
                return orden(UI_ACC_IR_A, h.x_um);
            }
        }
        break;
    }

    case UI_PANTALLA_MOVER:
        /* 40 µm por cada punto de la palanca y llamada (≈ 200 mm/s a tope a 20 ms). */
        mover_um += (int32_t)e->joy_x * 40;
        if (mover_um < 0) { mover_um = 0; }
        if (mover_um > EJE_LONGITUD_UM) { mover_um = EJE_LONGITUD_UM; }
        if (e->b4 || e->b4_largo) {
            ir_a_pantalla(UI_PANTALLA_PRINCIPAL);
        } else if (e->ok) {
            ir_a_pantalla(UI_PANTALLA_PRINCIPAL);
            return orden(UI_ACC_IR_A, mover_um);
        }
        break;

    case UI_PANTALLA_INFO:
        if (e->b4 || e->b4_largo || e->ok) {
            ir_a_pantalla(UI_PANTALLA_PRINCIPAL);
        }
        break;
    }
    return orden(UI_ACC_NINGUNA, 0);
}

/* ================================================================== */
/* Utilidades de dibujo                                                */
/* ================================================================== */

/* Texto negro o blanco, según cuál contraste más con el fondo. */
static uint16_t contraste(color_rgb_t c)
{
    unsigned luz = (c.r * 299u + c.g * 587u + c.b * 114u) / 1000u;
    return luz > 128u ? ST7789_NEGRO : ST7789_BLANCO;
}

/* "123.4" o "---.-" si se desconoce. */
static void fmt_mm(char *buf, size_t n, int32_t um)
{
    if (um < 0) {
        snprintf(buf, n, "---.-");
        return;
    }
    long decimas = (um + 50) / 100;
    snprintf(buf, n, "%ld.%ld", decimas / 10, decimas % 10);
}

static void dibujar_barra_titulo(const char *titulo)
{
    st7789_rect(0, 0, TFT_ANCHO, BARRA_ALTO, C_PANEL);
    texto_dibujar(MARGEN, 3, titulo, 2, C_TEXTO, C_PANEL);
}

static void dibujar_pie(const char *texto)
{
    texto_formato(MARGEN, 226, 1, C_TENUE, C_FONDO, 38, "%s", texto);
}

static void dibujar_escala(int y, int alto)
{
    static uint16_t columna[24];
    for (int x = 0; x < ANCHO_UTIL; x++) {
        int h = (ESCALA_TONO_INICIAL_GRADOS + x * 360 / ANCHO_UTIL) % 360;
        color_hsv_t hsv = { (int16_t)h, 255, 255 };
        color_rgb_t c = color_desde_hsv(hsv);
        for (int i = 0; i < alto; i++) {
            columna[i] = ST7789_RGB565(c.r, c.g, c.b);
        }
        st7789_bloque(MARGEN + x, y, 1, alto, columna);
    }
}

static int um_a_px(int32_t um)
{
    int px = (int)((int64_t)(um - ESCALA_DESPLAZAMIENTO_UM) * ANCHO_UTIL / ESCALA_ANCHO_UM);
    if (px < 0) { px = 0; }
    if (px > ANCHO_UTIL - 1) { px = ANCHO_UTIL - 1; }
    return MARGEN + px;
}

/* Marca vertical; se borra la anterior solo si cambió de columna. */
static void dibujar_marca(int *px_anterior, int32_t um, int y, int alto, uint16_t color)
{
    int px = um < 0 ? -1 : um_a_px(um);
    if (px == *px_anterior) {
        return;
    }
    if (*px_anterior >= 0) {
        st7789_rect(*px_anterior - 1, y, 3, alto, C_FONDO);
    }
    if (px >= 0) {
        st7789_rect(px - 1, y, 3, alto, color);
    }
    *px_anterior = px;
}

/* Ancho de un chip de n caracteres: n·6 − 1 de texto + 3 px de margen a cada lado. */
#define CHIP_ANCHO(n)  ((n) * 6 - 1 + 6)

static void chip(int x, int y, const char *texto, bool activo, uint16_t color_activo)
{
    int w = CHIP_ANCHO(texto_longitud(texto));
    st7789_rect(x, y, w, 11, activo ? color_activo : C_CHIP);
    texto_dibujar(x + 3, y + 2, texto, 1, activo ? C_FONDO : C_TENUE, activo ? color_activo : C_CHIP);
}

/* ================================================================== */
/* Pantalla principal                                                  */
/* ================================================================== */

#define P_MUESTRA_Y     24
#define P_MUESTRA_ALTO  72
#define P_VALORES_Y     100
#define P_HSV_Y         120
#define P_POS_Y         140
#define P_OBJ_Y         158
#define P_ESCALA_Y      184
#define P_ESCALA_ALTO   14
#define P_MARCA_OBJ_Y   176
#define P_MARCA_POS_Y   199
#define P_MARCA_ALTO    7
#define P_ESTADO_Y      210
#define P_AYUDA_Y       222
#define P_MENSAJE_Y     232

static void principal_estatico(void)
{
    dibujar_escala(P_ESCALA_Y, P_ESCALA_ALTO);
    marca_obj_px = marca_pos_px = -1;
}

static const char *texto_clase(escala_clase_t c)
{
    switch (c) {
    case ESCALA_CROMATICO: return "con tono";
    case ESCALA_NEGRO:     return "negro (sin tono)";
    case ESCALA_BLANCO:    return "blanco (sin tono)";
    case ESCALA_GRIS:      return "gris (sin tono)";
    }
    return "";
}

static void principal_seccion(const ui_modelo_t *m, unsigned s)
{
    char a[16];

    switch (s) {
    case 0: /* barra de estado */
        st7789_rect(0, 0, TFT_ANCHO, BARRA_ALTO, C_PANEL);
        texto_formato(MARGEN, 3, 2, m->estado_color, C_PANEL, 0, "%s", m->estado);
        {
            int x = TFT_ANCHO - 4;
            x -= CHIP_ANCHO(3) + 3; chip(x, 6, "ARM", m->chip_arm, C_OK);
            x -= CHIP_ANCHO(3) + 3; chip(x, 6, "REF", m->chip_ref, C_OK);
            x -= CHIP_ANCHO(3) + 3; chip(x, 6, "CAL", m->chip_cal, C_OK);
            x -= CHIP_ANCHO(4) + 3; chip(x, 6, "WiFi", m->chip_wifi, C_ACENTO);
        }
        break;

    case 1: /* muestra del color y HEX */
        if (m->hay_lectura) {
            uint16_t c = ST7789_RGB565(m->rgb.r, m->rgb.g, m->rgb.b);
            uint16_t t = contraste(m->rgb);
            st7789_rect(MARGEN, P_MUESTRA_Y, ANCHO_UTIL, P_MUESTRA_ALTO, c);
            texto_centrado(MARGEN, ANCHO_UTIL, P_MUESTRA_Y + 18, m->hex, 3, t, c);
            texto_centrado(MARGEN, ANCHO_UTIL, P_MUESTRA_Y + 56, texto_clase(m->clase), 1, t, c);
        } else {
            st7789_rect(MARGEN, P_MUESTRA_Y, ANCHO_UTIL, P_MUESTRA_ALTO, C_PANEL);
            texto_centrado(MARGEN, ANCHO_UTIL, P_MUESTRA_Y + 20, "SIN LECTURA", 2, C_TENUE, C_PANEL);
            texto_centrado(MARGEN, ANCHO_UTIL, P_MUESTRA_Y + 46, "pulsa B1 para medir", 1, C_TENUE, C_PANEL);
        }
        break;

    case 2: /* R G B y H S V */
        if (m->hay_lectura) {
            texto_formato(MARGEN,       P_VALORES_Y, 2, C_R, C_FONDO, 5, "R:%3u", m->rgb.r);
            texto_formato(MARGEN + 84,  P_VALORES_Y, 2, C_G, C_FONDO, 5, "G:%3u", m->rgb.g);
            texto_formato(MARGEN + 168, P_VALORES_Y, 2, C_B, C_FONDO, 5, "B:%3u", m->rgb.b);
            if (m->h >= 0) {
                texto_formato(MARGEN, P_HSV_Y, 2, C_TEXTO, C_FONDO, 6, "H:%3d°", m->h);
            } else {
                texto_formato(MARGEN, P_HSV_Y, 2, C_TENUE, C_FONDO, 6, "H:---");
            }
            texto_formato(MARGEN + 84,  P_HSV_Y, 2, C_TEXTO, C_FONDO, 6, "S:%u%%", m->s * 100u / 255u);
            texto_formato(MARGEN + 168, P_HSV_Y, 2, C_TEXTO, C_FONDO, 6, "V:%u%%", m->v * 100u / 255u);
        } else {
            /* Mismas columnas que los valores reales: si no, al medir quedan restos. */
            texto_formato(MARGEN,       P_VALORES_Y, 2, C_TENUE, C_FONDO, 5, "R:---");
            texto_formato(MARGEN + 84,  P_VALORES_Y, 2, C_TENUE, C_FONDO, 5, "G:---");
            texto_formato(MARGEN + 168, P_VALORES_Y, 2, C_TENUE, C_FONDO, 5, "B:---");
            texto_formato(MARGEN,       P_HSV_Y, 2, C_TENUE, C_FONDO, 6, "H:---");
            texto_formato(MARGEN + 84,  P_HSV_Y, 2, C_TENUE, C_FONDO, 6, "S:---");
            texto_formato(MARGEN + 168, P_HSV_Y, 2, C_TENUE, C_FONDO, 6, "V:---");
        }
        break;

    case 3: /* posición y objetivo */
        fmt_mm(a, sizeof a, m->pos_um);
        texto_formato(MARGEN, P_POS_Y, 2, C_TEXTO, C_FONDO, 19, "Pos %s mm", a);
        if (m->objetivo_um >= 0) {
            fmt_mm(a, sizeof a, m->objetivo_um);
            texto_formato(MARGEN, P_OBJ_Y, 2, C_ACENTO, C_FONDO, 19, "Obj %s mm", a);
        } else {
            texto_formato(MARGEN, P_OBJ_Y, 2, C_TENUE, C_FONDO, 19, "Obj ---");
        }
        break;

    case 4: /* marcas sobre la escala */
        dibujar_marca(&marca_obj_px, m->objetivo_um, P_MARCA_OBJ_Y, P_MARCA_ALTO, C_TEXTO);
        dibujar_marca(&marca_pos_px, m->pos_um, P_MARCA_POS_Y, P_MARCA_ALTO, C_AVISO);
        break;

    default: /* estado del eje, sensores, ayuda y mensaje */
        texto_formato(MARGEN, P_ESTADO_Y, 1, C_TENUE, C_FONDO, 38, "Eje:%s  IR:%c%c  Prox:%s",
                      m->eje, m->ir_inicio ? 'I' : '-', m->ir_fin ? 'F' : '-',
                      m->prox[0] ? m->prox : "--");
        texto_formato(MARGEN, P_AYUDA_Y, 1, C_TENUE, C_FONDO, 38, "B1 Medir B2 Calib B3 Mover B4 Guardar");
        if (m->error[0]) {
            texto_formato(MARGEN, P_MENSAJE_Y, 1, C_ERROR, C_FONDO, 38, "%s", m->error);
        } else if (m->mensaje[0]) {
            texto_formato(MARGEN, P_MENSAJE_Y, 1, C_ACENTO, C_FONDO, 38, "%s", m->mensaje);
        } else {
            texto_formato(MARGEN, P_MENSAJE_Y, 1, C_TENUE, C_FONDO, 38, "SW o B4 largo: menú");
        }
        break;
    }
}
#define PRINCIPAL_SECCIONES 6

/* ================================================================== */
/* Menú                                                                */
/* ================================================================== */

static const char *nombre_item(item_menu_t i, const ui_modelo_t *m)
{
    switch (i) {
    case M_MEDIR:       return "Medir color";
    case M_CAL_NEGRO:   return "Calibrar negro";
    case M_CAL_BLANCO:  return "Calibrar blanco";
    case M_REFERENCIAR: return "Referenciar eje";
    case M_MOVER:       return "Mover cursor";
    case M_MANUAL:      return "Modo manual";
    case M_HISTORIAL:   return "Historial";
    case M_ARMAR:       return m->armado ? "Desarmar motor" : "Armar motor";
    case M_INFO:        return "Información";
    case M_CANTIDAD:    break;
    }
    return "";
}

static void menu_seccion(const ui_modelo_t *m, unsigned s)
{
    if (s >= M_CANTIDAD) {
        dibujar_pie("Joystick: elegir  SW: ok  B4: salir");
        return;
    }
    bool sel = (int)s == menu_sel;
    uint16_t bg = sel ? C_SEL : C_FONDO;
    st7789_rect(0, 30 + (int)s * FILA_MENU, TFT_ANCHO, FILA_MENU, bg);
    texto_formato(10, 32 + (int)s * FILA_MENU, 2, sel ? C_TEXTO : C_TENUE, bg, 0, "%s %s",
                  sel ? ">" : " ", nombre_item((item_menu_t)s, m));
}
#define MENU_SECCIONES (M_CANTIDAD + 1)

/* ================================================================== */
/* Historial                                                           */
/* ================================================================== */

#define H_FILAS 7

static void historial_seccion(const ui_modelo_t *m, unsigned s)
{
    int n = m->historial ? historial_cantidad(m->historial) : 0;

    if (s >= H_FILAS) {
        dibujar_pie(n > 0 ? "SW: ir a esa posición  B4: volver" : "B4: volver");
        return;
    }
    if (n == 0) {
        if (s == 2) {
            texto_centrado(0, TFT_ANCHO, 80, "Sin lecturas", 2, C_TENUE, C_FONDO);
            texto_centrado(0, TFT_ANCHO, 106, "guarda con B4 al medir", 1, C_TENUE, C_FONDO);
        }
        return;
    }

    /* Ventana de H_FILAS entradas que siempre contiene la seleccionada. */
    int primera = hist_sel < H_FILAS ? 0 : hist_sel - (H_FILAS - 1);
    int idx = primera + (int)s;
    int y = 30 + (int)s * FILA_MENU;

    if (idx >= n) {
        st7789_rect(0, y, TFT_ANCHO, FILA_MENU, C_FONDO);
        return;
    }
    entrada_hist_t e;
    if (!historial_obtener(m->historial, (uint8_t)idx, &e)) {
        return;
    }
    bool sel = idx == hist_sel;
    uint16_t bg = sel ? C_SEL : C_FONDO;
    char hex[8], pos[16];
    color_a_hex(e.rgb, hex);
    fmt_mm(pos, sizeof pos, e.x_um);

    st7789_rect(0, y, TFT_ANCHO, FILA_MENU, bg);
    st7789_rect(8, y + 2, 16, 16, ST7789_RGB565(e.rgb.r, e.rgb.g, e.rgb.b));
    texto_dibujar(32, y + 2, hex, 2, sel ? C_TEXTO : C_TENUE, bg);
    if (e.x_um >= 0) {
        texto_formato(128, y + 2, 2, sel ? C_TEXTO : C_TENUE, bg, 7, "%smm", pos);
    } else {
        texto_formato(128, y + 2, 2, C_TENUE, bg, 7, "sin pos.");
    }
}
#define HISTORIAL_SECCIONES (H_FILAS + 1)

/* ================================================================== */
/* Mover cursor                                                        */
/* ================================================================== */

#define V_ESCALA_Y   150
#define V_ESCALA_ALTO 20
#define V_MARCA_OBJ_Y 134
#define V_MARCA_POS_Y 172
#define V_MARCA_ALTO  14

static void mover_estatico(void)
{
    dibujar_escala(V_ESCALA_Y, V_ESCALA_ALTO);
    marca_obj_px = marca_pos_px = -1;
}

static void mover_seccion(const ui_modelo_t *m, unsigned s)
{
    char a[16];

    switch (s) {
    case 0:
        fmt_mm(a, sizeof a, mover_um);
        texto_formato(MARGEN, 52, 4, C_TEXTO, C_FONDO, 5, "%s", a);
        texto_dibujar(MARGEN + 5 * 24 + 6, 68, "mm", 2, C_TENUE, C_FONDO);
        break;
    case 1:
        fmt_mm(a, sizeof a, m->pos_um);
        texto_formato(MARGEN, 100, 2, C_TENUE, C_FONDO, 19, "Actual %s mm", a);
        break;
    case 2:
        dibujar_marca(&marca_obj_px, mover_um, V_MARCA_OBJ_Y, V_MARCA_ALTO, C_TEXTO);
        dibujar_marca(&marca_pos_px, m->pos_um, V_MARCA_POS_Y, V_MARCA_ALTO, C_AVISO);
        break;
    default:
        dibujar_pie("Joystick X: ajustar  SW: ir  B4: no");
        break;
    }
}
#define MOVER_SECCIONES 4

/* ================================================================== */
/* Información                                                         */
/* ================================================================== */

static void info_seccion(const ui_modelo_t *m, unsigned s)
{
    const int x = MARGEN, paso = 14, y0 = 32;
    int y = y0 + (int)s * paso;

    switch (s) {
    case 0:
        texto_formato(x, y, 1, C_TEXTO, C_FONDO, 38, "Color negro:   %s", m->hay_negro ? "calibrado" : "pendiente");
        break;
    case 1:
        texto_formato(x, y, 1, C_TEXTO, C_FONDO, 38, "Color blanco:  %s", m->hay_blanco ? "calibrado" : "pendiente");
        break;
    case 2:
        if (m->recorrido_ms > 0) {
            texto_formato(x, y, 1, C_TEXTO, C_FONDO, 38, "Recorrido:     %lu ms (%d mm)",
                          (unsigned long)m->recorrido_ms, EJE_LONGITUD_UM / 1000);
        } else {
            texto_formato(x, y, 1, C_TEXTO, C_FONDO, 38, "Recorrido:     sin calibrar");
        }
        break;
    case 3:
        texto_formato(x, y, 1, C_TEXTO, C_FONDO, 38, "Eje:           %s", m->eje);
        break;
    case 4:
        texto_formato(x, y, 1, C_TEXTO, C_FONDO, 38, "Finales IR:    inicio %s, fin %s",
                      m->ir_inicio ? "ACTIVO" : "libre", m->ir_fin ? "ACTIVO" : "libre");
        break;
    case 5:
        texto_formato(x, y, 1, C_TEXTO, C_FONDO, 38, "IR proximidad: %s", m->prox[0] ? m->prox : "--");
        break;
    case 6:
        texto_formato(x, y, 1, C_TENUE, C_FONDO, 38, "Wi-Fi:         reservado (sin usar)");
        break;
    case 7:
        texto_formato(x, y, 1, m->error[0] ? C_ERROR : C_OK, C_FONDO, 38, "%s",
                      m->error[0] ? m->error : "Sin errores");
        break;
    case 8:
        texto_formato(x, y + 6, 1, C_TENUE, C_FONDO, 38, "Firmware v1.0 - RP2040 por registros");
        break;
    default:
        dibujar_pie("B4 o SW: volver");
        break;
    }
}
#define INFO_SECCIONES 10

/* ================================================================== */
/* Orquestación del dibujo                                             */
/* ================================================================== */

static const char *titulo_pantalla(void)
{
    switch (pantalla) {
    case UI_PANTALLA_MENU:      return "Menú";
    case UI_PANTALLA_HISTORIAL: return "Historial";
    case UI_PANTALLA_MOVER:     return "Mover cursor";
    case UI_PANTALLA_INFO:      return "Información";
    default:                    return "";
    }
}

void ui_dibujar(const ui_modelo_t *m)
{
    if (pantalla_nueva) {
        pantalla_nueva = false;
        st7789_rellenar(C_FONDO);
        if (pantalla != UI_PANTALLA_PRINCIPAL) {
            dibujar_barra_titulo(titulo_pantalla());
        }
        if (pantalla == UI_PANTALLA_PRINCIPAL) { principal_estatico(); }
        if (pantalla == UI_PANTALLA_MOVER)     { mover_estatico(); }
        seccion = 0;
        return;     /* el borrado ya costó ≈ 15 ms: lo demás, en las próximas llamadas */
    }

    unsigned total = 1;
    switch (pantalla) {
    case UI_PANTALLA_PRINCIPAL: total = PRINCIPAL_SECCIONES; principal_seccion(m, seccion); break;
    case UI_PANTALLA_MENU:      total = MENU_SECCIONES;      menu_seccion(m, seccion);      break;
    case UI_PANTALLA_HISTORIAL: total = HISTORIAL_SECCIONES; historial_seccion(m, seccion); break;
    case UI_PANTALLA_MOVER:     total = MOVER_SECCIONES;     mover_seccion(m, seccion);     break;
    case UI_PANTALLA_INFO:      total = INFO_SECCIONES;      info_seccion(m, seccion);      break;
    }
    seccion = (seccion + 1) % total;
}
