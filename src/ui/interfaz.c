#include "ui/interfaz.h"

#include <stdio.h>
#include <string.h>

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
/* El diseño base es de 240 px de alto. En un panel más alto (240x320) el contenido del medio
   baja EXTRA_M y los textos de abajo bajan EXTRA, para aprovechar toda la pantalla. */
#define EXTRA         (TFT_ALTO - 240)
#define EXTRA_M       (EXTRA / 2)
#define FILA_MENU     (20 + EXTRA / 10)
#define TXT_Y(y)      ((y) + (FILA_MENU - 16) / 2)      /* texto de escala 2 centrado en una fila */

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

void ui_abrir_asistente(void)
{
    pantalla = UI_PANTALLA_ASISTENTE;
    pantalla_nueva = true;
    seccion = 0;
}

/* Paso del asistente según el estado del sistema: 0 referenciar el eje, 1 calibrar
   el negro, 2 calibrar el blanco, 3 listo. */
static int paso_asistente(const ui_modelo_t *m)
{
    if (!m->chip_ref)   { return 0; }
    if (!m->hay_negro)  { return 1; }
    if (!m->hay_blanco) { return 2; }
    return 3;
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
    ui_orden_t o = { a, um, false };
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

/* Navegar por listas: el joystick arriba/abajo, o B2 (arriba) y B3 (abajo). */
static int navegacion(const ui_entrada_t *e)
{
    if (e->b2) { return -1; }
    if (e->b3) { return +1; }
    return e->dir;
}

static int rotar(int valor, int paso, int cantidad)
{
    return (valor + paso + cantidad) % cantidad;
}

ui_orden_t ui_procesar(const ui_entrada_t *e, const ui_modelo_t *m)
{
    switch (pantalla) {
    case UI_PANTALLA_ASISTENTE: {
        int paso = paso_asistente(m);

        if (e->b4_largo) {                          /* omitir el asistente */
            ir_a_pantalla(UI_PANTALLA_PRINCIPAL);
            break;
        }
        if (m->midiendo) {
            break;
        }
        if (paso == 0 && e->b1) {
            return orden(UI_ACC_REFERENCIAR, 0);
        }
        if ((paso == 1 || paso == 2) && (e->b1 || e->b2)) {
            ui_orden_t o = orden(paso == 1 ? UI_ACC_CAL_NEGRO : UI_ACC_CAL_BLANCO, 0);
            /* El IR de proximidad no ve el negro: se mide sin esperarle. B2 fuerza también el blanco. */
            o.forzar = paso == 1 || e->b2;
            return o;
        }
        if (paso == 3 && e->b1) {
            ir_a_pantalla(UI_PANTALLA_PRINCIPAL);
        }
        break;
    }

    case UI_PANTALLA_PRINCIPAL:
        if (e->b1) {
            return orden(UI_ACC_MEDIR, 0);
        }
        if (e->b2) {
            menu_sel = M_CAL_NEGRO;
            ir_a_pantalla(UI_PANTALLA_MENU);
        } else if (e->b4_largo) {              /* B4 mantenido: guardar la lectura */
            return orden(UI_ACC_GUARDAR, 0);
        } else if (e->b4) {                    /* B4 corto: menú */
            menu_sel = M_MEDIR;
            ir_a_pantalla(UI_PANTALLA_MENU);
        }
        break;

    case UI_PANTALLA_MENU: {
        int dir = navegacion(e);
        if (dir) {
            menu_sel = rotar(menu_sel, dir, M_CANTIDAD);
        }
        if (e->b4 || e->b4_largo) {
            ir_a_pantalla(UI_PANTALLA_PRINCIPAL);
        } else if (e->b1) {
            return ejecutar_item((item_menu_t)menu_sel, m);
        }
        break;
    }

    case UI_PANTALLA_HISTORIAL: {
        int n = m->historial ? historial_cantidad(m->historial) : 0;
        int dir = navegacion(e);
        if (dir && n > 0) {
            hist_sel = rotar(hist_sel, dir, n);
        }
        if (e->b4 || e->b4_largo) {
            ir_a_pantalla(UI_PANTALLA_PRINCIPAL);
        } else if (e->b1 && n > 0) {
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
        } else if (e->b1) {
            ir_a_pantalla(UI_PANTALLA_PRINCIPAL);
            return orden(UI_ACC_IR_A, mover_um);
        }
        break;

    case UI_PANTALLA_INFO:
        if (e->b4 || e->b4_largo || e->b1) {
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
    texto_formato(MARGEN, TFT_ALTO - 14, 1, C_TENUE, C_FONDO, 38, "%s", texto);
}

/* Tres zonas, una por temperatura; el carrito para en el centro de cada una. */
static void dibujar_escala(int y, int alto)
{
    static const struct { const char *nombre; uint16_t fondo, texto; } zonas[3] = {
        { "CALIDO", ST7789_RGB565(235, 110, 50),  ST7789_NEGRO  },
        { "NEUTRO", ST7789_RGB565(140, 145, 155), ST7789_NEGRO  },
        { "FRIO",   ST7789_RGB565(60, 130, 235),  ST7789_BLANCO },
    };
    int ancho = ANCHO_UTIL / 3;

    for (int i = 0; i < 3; i++) {
        int x = MARGEN + i * ancho;
        st7789_rect(x, y, ancho, alto, zonas[i].fondo);
        texto_centrado(x, ancho, y + (alto - 7) / 2, zonas[i].nombre, 1, zonas[i].texto, zonas[i].fondo);
    }
}

static int um_a_px(int32_t um)
{
    int px = (int)((int64_t)(um - ESCALA_DESPLAZAMIENTO_UM) * ANCHO_UTIL / ESCALA_ANCHO_UM);
    if (px < 0) { px = 0; }
    if (px > ANCHO_UTIL - 1) { px = ANCHO_UTIL - 1; }
    return MARGEN + px;
}

/* Barra cromática: todos los tonos de 0° a 360° (rojo → amarillo → verde → cian → azul →
   magenta → rojo), de izquierda a derecha. Va debajo de la de cálido / neutro / frío. */
static void dibujar_columna_tono(int x, int y, int alto)
{
    static uint16_t columna[16];
    color_hsv_t hsv = { (int16_t)(x * 360 / ANCHO_UTIL), 255, 255 };
    color_rgb_t c = color_desde_hsv(hsv);

    for (int i = 0; i < alto && i < 16; i++) {
        columna[i] = ST7789_RGB565(c.r, c.g, c.b);
    }
    st7789_bloque(MARGEN + x, y, 1, alto, columna);
}

static void dibujar_barra_tonos(int y, int alto)
{
    for (int x = 0; x < ANCHO_UTIL; x++) {
        dibujar_columna_tono(x, y, alto);
    }
}

static int tono_a_px(int h)
{
    return MARGEN + h * ANCHO_UTIL / 360;
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
#define P_MUESTRA_ALTO  (72 + EXTRA_M)
#define P_VALORES_Y     (100 + EXTRA_M)
#define P_HSV_Y         (120 + EXTRA_M)
#define P_POS_Y         (140 + EXTRA_M)
#define P_OBJ_Y         (158 + EXTRA_M)
#define P_ESCALA_Y      (184 + EXTRA_M)
#define P_ESCALA_ALTO   14
#define P_MARCA_OBJ_Y   (176 + EXTRA_M)
#define P_MARCA_POS_Y   (199 + EXTRA_M)
#define P_MARCA_ALTO    7
#define P_ESTADO_Y      (210 + EXTRA)
#define P_AYUDA_Y       (222 + EXTRA)
#define P_MENSAJE_Y     (232 + EXTRA)

/* Barra cromática bajo la de temperaturas y su marca del tono medido. */
#define P_TONO_Y         (P_MARCA_POS_Y + P_MARCA_ALTO + 4)
#define P_TONO_ALTO      10

static int marca_tono_px = -1;

/* Cursor del tono: una línea blanca de 3 px que atraviesa la barra cromática y sobresale 3 px por
   arriba y por abajo, para que se vea bien dónde está el tono medido o elegido. Al moverla se
   restauran el fondo y las columnas de colores que tapaba. */
#define CURSOR_TONO_SALIDA 3

static void dibujar_cursor_tono(int *px_anterior, int px)
{
    if (px == *px_anterior) {
        return;
    }
    if (*px_anterior >= 0) {
        int a = *px_anterior;
        st7789_rect(a - 1, P_TONO_Y - CURSOR_TONO_SALIDA, 3, CURSOR_TONO_SALIDA, C_FONDO);
        st7789_rect(a - 1, P_TONO_Y + P_TONO_ALTO, 3, CURSOR_TONO_SALIDA, C_FONDO);
        for (int x = a - 1; x <= a + 1; x++) {
            if (x >= MARGEN && x < MARGEN + ANCHO_UTIL) {
                dibujar_columna_tono(x - MARGEN, P_TONO_Y, P_TONO_ALTO);
            }
        }
    }
    if (px >= 0) {
        st7789_rect(px - 1, P_TONO_Y - CURSOR_TONO_SALIDA, 3, P_TONO_ALTO + 2 * CURSOR_TONO_SALIDA, C_TEXTO);
    }
    *px_anterior = px;
}

static void principal_estatico(void)
{
    dibujar_escala(P_ESCALA_Y, P_ESCALA_ALTO);
    dibujar_barra_tonos(P_TONO_Y, P_TONO_ALTO);
    marca_obj_px = marca_pos_px = marca_tono_px = -1;
}

static const char *texto_clase(escala_clase_t c)
{
    switch (c) {
    case ESCALA_CROMATICO: return "con tono";
    case ESCALA_NEGRO:     return "negro";
    case ESCALA_BLANCO:    return "blanco";
    case ESCALA_GRIS:      return "gris";
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
            texto_centrado(MARGEN, ANCHO_UTIL, P_MUESTRA_Y + 18 + EXTRA_M / 2, m->hex, 3, t, c);
            char clase[24];
            snprintf(clase, sizeof clase, "%s: %s", texto_clase(m->clase), escala_temperatura_texto(m->temperatura));
            texto_centrado(MARGEN, ANCHO_UTIL, P_MUESTRA_Y + 56 + EXTRA_M, clase, 1, t, c);
        } else {
            st7789_rect(MARGEN, P_MUESTRA_Y, ANCHO_UTIL, P_MUESTRA_ALTO, C_PANEL);
            texto_centrado(MARGEN, ANCHO_UTIL, P_MUESTRA_Y + 20 + EXTRA_M / 2, "SIN LECTURA", 2, C_TENUE, C_PANEL);
            texto_centrado(MARGEN, ANCHO_UTIL, P_MUESTRA_Y + 46 + EXTRA_M, "pulsa B1 para medir", 1, C_TENUE, C_PANEL);
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
        /* tono medido sobre la barra cromática; sin marca en colores sin tono */
        dibujar_cursor_tono(&marca_tono_px, (m->hay_lectura && m->h >= 0) ? tono_a_px(m->h) : -1);
        break;

    default: /* estado del eje, sensores, ayuda y mensaje */
        texto_formato(MARGEN, P_ESTADO_Y, 1, C_TENUE, C_FONDO, 38, "Eje:%s  IR:%c%c  Prox:%s",
                      m->eje, m->ir_inicio ? 'I' : '-', m->ir_fin ? 'F' : '-',
                      m->prox[0] ? m->prox : "--");
        texto_formato(MARGEN, P_AYUDA_Y, 1, C_TENUE, C_FONDO, 38, "B1 Medir B2 Calib B4 largo: Guardar");
        if (m->error[0]) {
            texto_formato(MARGEN, P_MENSAJE_Y, 1, C_ERROR, C_FONDO, 38, "%s", m->error);
        } else if (m->mensaje[0]) {
            texto_formato(MARGEN, P_MENSAJE_Y, 1, C_ACENTO, C_FONDO, 38, "%s", m->mensaje);
        } else {
            texto_formato(MARGEN, P_MENSAJE_Y, 1, C_TENUE, C_FONDO, 38, "B2 o B4: menú  B4 largo: guardar");
        }
        break;
    }
}
#define PRINCIPAL_SECCIONES 6

/* Cada sección de la pantalla principal se repinta solo si cambió lo que muestra.
   Repintar sin cambios hacía parpadear la barra de estado y la tarjeta del color
   (se pinta el fondo y luego el texto encima, en cada ciclo). */
#define FIRMAS_MAX 24
static uint32_t firma_seccion[FIRMAS_MAX];
static bool     firma_valida[FIRMAS_MAX];

static uint32_t mezclar(uint32_t h, const void *datos, size_t n)
{
    const uint8_t *p = datos;
    while (n--) {
        h ^= *p++;
        h *= 16777619u;
    }
    return h;
}

#define MEZCLAR(h, x)      mezclar((h), &(x), sizeof(x))
#define MEZCLAR_TEXTO(h, t) mezclar((h), (t), strlen(t))

static uint32_t firma_principal(const ui_modelo_t *m, unsigned s)
{
    uint32_t h = 2166136261u;

    switch (s) {
    case 0:
        h = MEZCLAR_TEXTO(h, m->estado);
        h = MEZCLAR(h, m->estado_color);
        h = MEZCLAR(h, m->chip_arm); h = MEZCLAR(h, m->chip_ref);
        h = MEZCLAR(h, m->chip_cal); h = MEZCLAR(h, m->chip_wifi);
        break;
    case 1:
        h = MEZCLAR(h, m->hay_lectura);
        h = MEZCLAR(h, m->rgb);
        h = MEZCLAR_TEXTO(h, m->hex);
        h = MEZCLAR(h, m->clase);
        h = MEZCLAR(h, m->temperatura);
        break;
    case 2:
        h = MEZCLAR(h, m->hay_lectura);
        h = MEZCLAR(h, m->rgb);
        h = MEZCLAR(h, m->h); h = MEZCLAR(h, m->s); h = MEZCLAR(h, m->v);
        break;
    case 3:
    case 4:
        h = MEZCLAR(h, m->pos_um);
        h = MEZCLAR(h, m->objetivo_um);
        h = MEZCLAR(h, m->hay_lectura);
        h = MEZCLAR(h, m->h);
        break;
    default:
        h = MEZCLAR_TEXTO(h, m->eje);
        h = MEZCLAR(h, m->ir_inicio); h = MEZCLAR(h, m->ir_fin);
        h = MEZCLAR_TEXTO(h, m->prox);
        h = MEZCLAR_TEXTO(h, m->error);
        h = MEZCLAR_TEXTO(h, m->mensaje);
        break;
    }
    return h;
}

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
        dibujar_pie("B2/B3: elegir  B1/SW: ok  B4: salir");
        return;
    }
    bool sel = (int)s == menu_sel;
    uint16_t bg = sel ? C_SEL : C_FONDO;
    st7789_rect(0, 30 + (int)s * FILA_MENU, TFT_ANCHO, FILA_MENU, bg);
    texto_formato(10, TXT_Y(30 + (int)s * FILA_MENU), 2, sel ? C_TEXTO : C_TENUE, bg, 0, "%s %s",
                  sel ? ">" : " ", nombre_item((item_menu_t)s, m));
}
#define MENU_SECCIONES (M_CANTIDAD + 1)

/* ================================================================== */
/* Historial                                                           */
/* ================================================================== */

#define H_FILAS ((TFT_ALTO - 60) / FILA_MENU)

static void historial_seccion(const ui_modelo_t *m, unsigned s)
{
    int n = m->historial ? historial_cantidad(m->historial) : 0;

    if (s >= H_FILAS) {
        dibujar_pie(n > 0 ? "B2/B3: elegir  B1: ir  B4: volver" : "B4: volver");
        return;
    }
    if (n == 0) {
        if (s == 2) {
            texto_centrado(0, TFT_ANCHO, 80 + EXTRA_M, "Sin lecturas", 2, C_TENUE, C_FONDO);
            texto_centrado(0, TFT_ANCHO, 106 + EXTRA_M, "guarda con B4 largo al medir", 1, C_TENUE, C_FONDO);
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
    st7789_rect(8, TXT_Y(y), 16, 16, ST7789_RGB565(e.rgb.r, e.rgb.g, e.rgb.b));
    texto_dibujar(32, TXT_Y(y), hex, 2, sel ? C_TEXTO : C_TENUE, bg);
    if (e.x_um >= 0) {
        texto_formato(128, TXT_Y(y), 2, sel ? C_TEXTO : C_TENUE, bg, 7, "%smm", pos);
    } else {
        texto_formato(128, TXT_Y(y), 2, C_TENUE, bg, 7, "sin pos.");
    }
}
#define HISTORIAL_SECCIONES (H_FILAS + 1)

/* ================================================================== */
/* Mover cursor                                                        */
/* ================================================================== */

#define V_ESCALA_Y   (150 + EXTRA_M)
#define V_ESCALA_ALTO 20
#define V_MARCA_OBJ_Y (134 + EXTRA_M)
#define V_MARCA_POS_Y (172 + EXTRA_M)
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
        texto_formato(MARGEN, 52 + EXTRA_M / 2, 4, C_TEXTO, C_FONDO, 5, "%s", a);
        texto_dibujar(MARGEN + 5 * 24 + 6, 68 + EXTRA_M / 2, "mm", 2, C_TENUE, C_FONDO);
        break;
    case 1:
        fmt_mm(a, sizeof a, m->pos_um);
        texto_formato(MARGEN, 100 + EXTRA_M, 2, C_TENUE, C_FONDO, 19, "Actual %s mm", a);
        break;
    case 2:
        dibujar_marca(&marca_obj_px, mover_um, V_MARCA_OBJ_Y, V_MARCA_ALTO, C_TEXTO);
        dibujar_marca(&marca_pos_px, m->pos_um, V_MARCA_POS_Y, V_MARCA_ALTO, C_AVISO);
        break;
    default:
        dibujar_pie("Joystick X: ajustar  B1: ir  B4: no");
        break;
    }
}
#define MOVER_SECCIONES 4

/* ================================================================== */
/* Información                                                         */
/* ================================================================== */

static void info_seccion(const ui_modelo_t *m, unsigned s)
{
    const int x = MARGEN, paso = 14 + EXTRA / 20, y0 = 32;
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
        texto_formato(x, y + 6, 1, C_TENUE, C_FONDO, 38, "Firmware v2.0 - RP2040 por registros");
        break;
    case 9:
        texto_formato(x, TFT_ALTO - 108, 1, C_ACENTO, C_FONDO, 38, "Detector de color - Grupo 1");
        break;
    case 10: case 11: case 12: case 13: case 14: {
        static const char *const integrantes[5] = {
            "Erick Orlando Rashon Gonzalez",
            "Werner Josue Ortiz Abdalla",
            "Brayan Kenet Rivera Quinilla",
            "Wilder Alfonso Castillo Salguero",
            "Marco Tulio Pineda Recinos",
        };
        texto_formato(x, TFT_ALTO - 94 + ((int)s - 10) * 12, 1, C_TEXTO, C_FONDO, 38, "%s", integrantes[s - 10]);
        break;
    }
    default:
        dibujar_pie("B1 o B4: volver");
        break;
    }
}
#define INFO_SECCIONES 16

/* ================================================================== */
/* Orquestación del dibujo                                             */
/* ================================================================== */

/* ================================================================== */
/* Asistente de inicio                                                 */
/* ================================================================== */

#define ASISTENTE_SECCIONES 5

static uint32_t firma_asistente(const ui_modelo_t *m, unsigned s)
{
    uint32_t h = 2166136261u;
    int paso = paso_asistente(m);

    h = MEZCLAR(h, paso);
    h = MEZCLAR(h, s);
    h = MEZCLAR(h, m->midiendo);
    h = MEZCLAR_TEXTO(h, m->estado);
    h = MEZCLAR_TEXTO(h, m->error);
    h = MEZCLAR_TEXTO(h, m->mensaje);
    return h;
}

static void asistente_seccion(const ui_modelo_t *m, unsigned s)
{
    static const char *const titulo[4] = { "REFERENCIAR EJE", "CALIBRAR NEGRO", "CALIBRAR BLANCO", "LISTO" };
    static const char *const texto[4][3] = {
        { "El carrito busca el INICIO y",   "mide el recorrido del eje.",  "No lo toques. B1/SW: empezar ya" },
        { "Pon la muestra NEGRA mate a la", "distancia de medir.",         "Pulsa B1 o SW para calibrar." },
        { "Pon el papel BLANCO a la misma", "distancia que el negro.",     "Pulsa B1 o SW para calibrar." },
        { "Calibración completa.",          "Ya puedes medir colores.",    "Pulsa B1 o SW para empezar." },
    };
    int paso = paso_asistente(m);

    switch (s) {
    case 0:
        if (paso == 3) {
            texto_formato(MARGEN, 34, 1, C_OK, C_FONDO, 38, "ASISTENTE COMPLETADO");
        } else {
            texto_formato(MARGEN, 34, 1, C_ACENTO, C_FONDO, 38, "PASO %d DE 3", paso + 1);
        }
        break;
    case 1:
        texto_formato(MARGEN, 56 + EXTRA / 4, 2, paso == 3 ? C_OK : C_TEXTO, C_FONDO, 19, "%s", titulo[paso]);
        break;
    case 2:
        for (int i = 0; i < 3; i++) {
            texto_formato(MARGEN, 90 + EXTRA / 4 + i * 16, 1, C_TEXTO, C_FONDO, 38, "%s", texto[paso][i]);
        }
        break;
    case 3: {
        const char *estado = m->midiendo ? "Analizando..." : m->estado;
        texto_formato(MARGEN, 150 + EXTRA / 3, 1, C_TENUE, C_FONDO, 38, "Estado: %s", estado);
        if (m->error[0]) {
            texto_formato(MARGEN, 166 + EXTRA / 3, 1, C_ERROR, C_FONDO, 38, "%s", m->error);
        } else {
            texto_formato(MARGEN, 166 + EXTRA / 3, 1, C_ACENTO, C_FONDO, 38, "%s", m->mensaje);
        }
        break;
    }
    default:
        if (paso == 1 || paso == 2) {
            texto_formato(MARGEN, TFT_ALTO - 30, 1, C_TENUE, C_FONDO, 38, "B2: calibrar sin esperar al IR");
        } else {
            texto_formato(MARGEN, TFT_ALTO - 30, 1, C_TENUE, C_FONDO, 38, "%s", "");
        }
        texto_formato(MARGEN, TFT_ALTO - 16, 1, C_TENUE, C_FONDO, 38, "B4 largo: omitir el asistente");
        break;
    }
}

static const char *titulo_pantalla(void)
{
    switch (pantalla) {
    case UI_PANTALLA_MENU:      return "Menú";
    case UI_PANTALLA_HISTORIAL: return "Historial";
    case UI_PANTALLA_MOVER:     return "Mover cursor";
    case UI_PANTALLA_INFO:      return "Información";
    case UI_PANTALLA_ASISTENTE: return "Asistente de inicio";
    default:                    return "";
    }
}

/* Firma común del menú, el historial, mover e información: cambia si cambia cualquier
   cosa que esas pantallas muestran o su selección. Así no se repintan sin necesidad. */
static uint32_t firma_pantalla(const ui_modelo_t *m)
{
    uint32_t h = 2166136261u;
    unsigned n_hist = m->historial ? historial_cantidad(m->historial) : 0u;

    h = MEZCLAR(h, pantalla);
    h = MEZCLAR(h, menu_sel);
    h = MEZCLAR(h, hist_sel);
    h = MEZCLAR(h, mover_um);
    h = MEZCLAR(h, n_hist);
    h = MEZCLAR(h, m->pos_um);
    h = MEZCLAR(h, m->objetivo_um);
    h = MEZCLAR(h, m->armado);
    h = MEZCLAR(h, m->hay_negro);
    h = MEZCLAR(h, m->hay_blanco);
    h = MEZCLAR(h, m->recorrido_ms);
    h = MEZCLAR(h, m->ir_inicio);
    h = MEZCLAR(h, m->ir_fin);
    h = MEZCLAR_TEXTO(h, m->eje);
    h = MEZCLAR_TEXTO(h, m->estado);
    h = MEZCLAR_TEXTO(h, m->prox);
    h = MEZCLAR_TEXTO(h, m->error);
    return h;
}

/* ¿Hay que pintar esta sección? Solo si su firma cambió desde la última vez. */
static bool seccion_cambiada(uint32_t f)
{
    if (firma_valida[seccion] && firma_seccion[seccion] == f) {
        return false;
    }
    firma_seccion[seccion] = f;
    firma_valida[seccion] = true;
    return true;
}

/* Secciones de la pantalla actual, para saber cuántas hay que pintar al abrirla. */
static unsigned secciones_pantalla(void)
{
    switch (pantalla) {
    case UI_PANTALLA_PRINCIPAL: return PRINCIPAL_SECCIONES;
    case UI_PANTALLA_ASISTENTE: return ASISTENTE_SECCIONES;
    case UI_PANTALLA_MENU:      return MENU_SECCIONES;
    case UI_PANTALLA_HISTORIAL: return HISTORIAL_SECCIONES;
    case UI_PANTALLA_MOVER:     return MOVER_SECCIONES;
    case UI_PANTALLA_INFO:      return INFO_SECCIONES;
    }
    return 1;
}

static unsigned construccion;       /* secciones que faltan por pintar tras abrir una pantalla */

bool ui_construyendo(void)
{
    return pantalla_nueva || construccion > 0;
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
        memset(firma_valida, 0, sizeof firma_valida);
        construccion = secciones_pantalla();
        return;     /* el borrado ya costó ≈ 15 ms: lo demás, en las próximas llamadas */
    }

    unsigned total = 1;
    switch (pantalla) {
    case UI_PANTALLA_PRINCIPAL:
        total = PRINCIPAL_SECCIONES;
        {
            uint32_t f = firma_principal(m, seccion);
            if (!firma_valida[seccion] || f != firma_seccion[seccion]) {
                principal_seccion(m, seccion);
                firma_seccion[seccion] = f;
                firma_valida[seccion] = true;
            }
        }
        break;
    case UI_PANTALLA_ASISTENTE:
        total = ASISTENTE_SECCIONES;
        {
            uint32_t f = firma_asistente(m, seccion);
            if (!firma_valida[seccion] || f != firma_seccion[seccion]) {
                asistente_seccion(m, seccion);
                firma_seccion[seccion] = f;
                firma_valida[seccion] = true;
            }
        }
        break;
    case UI_PANTALLA_MENU:
        total = MENU_SECCIONES;
        if (seccion_cambiada(firma_pantalla(m))) { menu_seccion(m, seccion); }
        break;
    case UI_PANTALLA_HISTORIAL:
        total = HISTORIAL_SECCIONES;
        if (seccion_cambiada(firma_pantalla(m))) { historial_seccion(m, seccion); }
        break;
    case UI_PANTALLA_MOVER:
        total = MOVER_SECCIONES;
        if (seccion_cambiada(firma_pantalla(m))) { mover_seccion(m, seccion); }
        break;
    case UI_PANTALLA_INFO:
        total = INFO_SECCIONES;
        if (seccion_cambiada(firma_pantalla(m))) { info_seccion(m, seccion); }
        break;
    }
    seccion = (seccion + 1) % total;
    if (construccion > 0) {
        construccion--;
    }
}
