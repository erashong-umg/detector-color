/* Pruebas de la máquina de estados con el código REAL de sistema.c, eje.c, carro.c,
   deteccion.c y proximidad (dominio y driver), contra motor, finales, sensor de color
   e IR de proximidad simulados. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "aplicacion/sistema.h"
#include "aplicacion/carro.h"
#include "aplicacion/eje.h"
#include "configuracion/eje.h"
#include "configuracion/proximidad.h"
#include "configuracion/sistema.h"
#include "dominio/proximidad.h"
#include "drivers/finales.h"
#include "drivers/tcs3200.h"
#include "hw/tiempo.h"

uint32_t mock_us;
extern int planta_dir;
extern double planta_duty;
extern int mock_adc_prox, mock_od_nivel;
extern tcs_frecuencias_t mock_frecuencias;
extern int mock_tcs_desborde, mock_tcs_llamadas;

double planta_x_um;
int ini_roto, fin_roto;
extern int forzar_ini, forzar_fin;

/* ---- planta física (misma que harness_eje) ---- */
#define LARGO 350000.0
static double K, ZONA_MUERTA = 15.0;
static int choque;

static void planta_inicio(double x0)
{
    unsigned duty40 = 25u + 75u * (EJE_VEL_VIAJE - 1) / 99u;
    K = LARGO / (8.0 * 1000.0) / ((double)duty40 - ZONA_MUERTA);
    planta_x_um = x0;
    ini_roto = fin_roto = 0;
    choque = 0;
}

static void planta_paso(void)
{
    double v = K * fmax(0.0, planta_duty - ZONA_MUERTA);
    planta_x_um += v * planta_dir;
    if (planta_x_um < -3000.0 || planta_x_um > LARGO + 3000.0) {
        choque = 1;
        planta_x_um = planta_x_um < 0 ? -3000.0 : LARGO + 3000.0;
    }
}

/* ---- infraestructura de pruebas ---- */
static int pasa, falla;
static int visto[32];               /* eventos vistos, por tipo */

#define COMPROBAR(cond, ...) do { \
    if (cond) { pasa++; } else { falla++; printf("   FALLO: "); printf(__VA_ARGS__); printf("  (línea %d)\n", __LINE__); } } while (0)

static void avanzar(int ms)
{
    for (int i = 0; i < ms; i++) {
        sistema_actualizar();
        planta_paso();
        mock_us += 1000;
        for (sistema_evento_t e; (e = sistema_tomar_evento()) != SIS_EVT_NINGUNO; ) {
            visto[e]++;
        }
    }
}

static int hasta_estado(sistema_estado_t e, int max_ms)
{
    int n = 0;
    while (sistema_estado() != e && n < max_ms) { avanzar(1); n++; }
    return n;
}

/* IR de proximidad: nivel orientado (mayor = más cerca) y salida digital. */
static void prox(prox_estado_t que)
{
    int nivel = 0, od_activo = 0;
    switch (que) {
    case PROX_SIN_OBJETO: nivel = 100;  od_activo = 0; break;
    case PROX_LEJOS:      nivel = 900;  od_activo = 0; break;
    case PROX_OK:         nivel = 2500; od_activo = 1; break;
    case PROX_CERCA:      nivel = 3900; od_activo = 1; break;
    }
    mock_adc_prox = PROX_ANALOGICO_INVERTIDO ? 4095 - nivel : nivel;
    mock_od_nivel = (PROX_DIGITAL_ACTIVO_BAJO ? !od_activo : od_activo) ? 1 : 0;
}

static const tcs_frecuencias_t NEGRO  = { 2000, 2000, 2000, 6000 };
static const tcs_frecuencias_t BLANCO = { 40000, 38000, 35000, 115000 };

static void color(int r, int g, int b)
{
    mock_frecuencias.r = NEGRO.r + (uint32_t)((BLANCO.r - NEGRO.r) * (r / 255.0));
    mock_frecuencias.g = NEGRO.g + (uint32_t)((BLANCO.g - NEGRO.g) * (g / 255.0));
    mock_frecuencias.b = NEGRO.b + (uint32_t)((BLANCO.b - NEGRO.b) * (b / 255.0));
    mock_frecuencias.c = NEGRO.c + (uint32_t)((BLANCO.c - NEGRO.c) * ((r + g + b) / 765.0));
}

static void reiniciar_todo(double x0)
{
    memset(visto, 0, sizeof visto);
    forzar_ini = forzar_fin = 0;
    planta_inicio(x0);
    prox(PROX_SIN_OBJETO);
    mock_tcs_desborde = 0;
    sistema_iniciar();
    avanzar(5);
}

static void armar_y_referenciar(void)
{
    sistema_armar(true);
    sistema_referenciar();
    hasta_estado(SIS_IDLE, 60000);
}

#define TITULO(t) printf("\n== %s\n", t)

int main(void)
{
    TITULO("1. Clasificación del IR con histéresis (dominio)");
    {
        prox_estado_t e = PROX_SIN_OBJETO;
        e = prox_clasificar(PROX_NIVEL_PRESENCIA + 50, false, e);   /* dentro de la histéresis: sigue sin objeto */
        COMPROBAR(e == PROX_SIN_OBJETO, "dentro de la histéresis no debe subir (%d)", e);
        e = prox_clasificar(PROX_NIVEL_PRESENCIA + PROX_HISTERESIS, false, e);
        COMPROBAR(e == PROX_LEJOS, "al superar umbral+histéresis sube a LEJOS (%d)", e);
        e = prox_clasificar(PROX_NIVEL_PRESENCIA - 50, false, e);
        COMPROBAR(e == PROX_LEJOS, "por encima de umbral−histéresis se mantiene LEJOS (%d)", e);
        e = prox_clasificar(PROX_NIVEL_PRESENCIA - PROX_HISTERESIS - 1, false, e);
        COMPROBAR(e == PROX_SIN_OBJETO, "al bajar de umbral−histéresis vuelve a SIN_OBJETO (%d)", e);
        e = prox_clasificar(0, true, PROX_SIN_OBJETO);
        COMPROBAR(e == PROX_LEJOS, "con OD activo es como mínimo LEJOS (%d)", e);
        e = prox_clasificar(4095, false, PROX_SIN_OBJETO);
        COMPROBAR(e == PROX_CERCA, "un nivel saturado salta directo a CERCA (%d)", e);
        e = prox_clasificar(PROX_NIVEL_OK + 10, false, PROX_LEJOS);
        COMPROBAR(e == PROX_LEJOS, "justo sobre OK, dentro de la histéresis, sigue LEJOS (%d)", e);

        /* filtro: un solo pico no cambia el estado */
        prox_filtro_t f; prox_filtro_iniciar(&f);
        for (int i = 0; i < 20; i++) prox_filtro_actualizar(&f, 100, false);
        prox_filtro_actualizar(&f, 2500, true);
        COMPROBAR(f.estado == PROX_SIN_OBJETO, "una lectura aislada no debe cambiar el estado");
        for (int i = 0; i < PROX_MUESTRAS_ESTABLES; i++) prox_filtro_actualizar(&f, 2500, true);
        COMPROBAR(f.estado == PROX_OK, "lecturas sostenidas sí lo cambian (%d)", f.estado);
        /* ruido alternando alrededor del umbral: no debe oscilar */
        int cambios = 0; prox_estado_t ant = f.estado;
        for (int i = 0; i < 200; i++) {
            prox_filtro_actualizar(&f, (i & 1) ? PROX_NIVEL_OK + 40 : PROX_NIVEL_OK - 40, true);
            if (f.estado != ant) { cambios++; ant = f.estado; }
        }
        COMPROBAR(cambios == 0, "el ruido en el umbral produjo %d cambios de estado", cambios);
    }

    TITULO("2. Arranque y órdenes rechazadas");
    reiniciar_todo(175000);
    COMPROBAR(sistema_estado() == SIS_IDLE, "tras INIT debe estar en IDLE (%s)", sistema_estado_texto(sistema_estado()));
    COMPROBAR(visto[SIS_EVT_LISTO] == 1, "falta el evento LISTO");
    COMPROBAR(!sistema_referenciar() && sistema_rechazo() == SIS_RECHAZO_NO_ARMADO, "referenciar sin armar debe rechazarse");
    COMPROBAR(!sistema_manual(true) && sistema_rechazo() == SIS_RECHAZO_NO_ARMADO, "manual sin armar debe rechazarse");
    COMPROBAR(!sistema_ir_a(100000) && sistema_rechazo() == SIS_RECHAZO_NO_ARMADO, "ir_a sin armar debe rechazarse");
    COMPROBAR(!sistema_medir() && sistema_rechazo() == SIS_RECHAZO_NO_CALIBRADO, "medir sin calibrar el color debe rechazarse");
    sistema_armar(true);
    COMPROBAR(!sistema_ir_a(100000) && sistema_rechazo() == SIS_RECHAZO_SIN_REFERENCIA, "ir_a sin referenciar debe rechazarse");

    TITULO("3. Homing y calibración del eje");
    reiniciar_todo(175000);
    sistema_armar(true);
    COMPROBAR(sistema_referenciar(), "referenciar debe aceptarse");
    COMPROBAR(sistema_estado() == SIS_HOMING, "debe pasar a HOMING");
    COMPROBAR(!sistema_medir() && sistema_rechazo() == SIS_RECHAZO_OCUPADO, "medir durante el homing debe rechazarse");
    int t = hasta_estado(SIS_IDLE, 80000);
    COMPROBAR(sistema_estado() == SIS_IDLE, "el homing no terminó en IDLE (%s tras %d ms)", sistema_estado_texto(sistema_estado()), t);
    COMPROBAR(visto[SIS_EVT_REFERENCIADO] == 1, "falta el evento REFERENCIADO");
    COMPROBAR(sistema_referenciado(), "el eje debe quedar calibrado");
    COMPROBAR(planta_x_um < 3000.0, "el cursor debe quedar junto al inicio (%.1f mm)", planta_x_um / 1000);
    COMPROBAR(!choque, "no debe haber choque mecánico");
    printf("   homing completo en %d ms, cursor en %.1f mm\n", t, planta_x_um / 1000);

    TITULO("4. Calibrar el color: guía y espera del objeto");
    /* sin objeto: guía COLOCAR y, tras el tiempo máximo, TIEMPO_AGOTADO */
    prox(PROX_SIN_OBJETO);
    COMPROBAR(sistema_calibrar(true), "calibrar negro debe aceptarse");
    avanzar(100);
    COMPROBAR(sistema_estado() == SIS_CALIBRATE, "debe estar en CALIBRATE");
    COMPROBAR(sistema_guia() == SIS_GUIA_COLOCAR_OBJETO, "sin objeto la guía debe ser COLOCAR (%d)", sistema_guia());
    COMPROBAR(mock_tcs_llamadas == 0, "no debe medir sin objeto");
    avanzar(SISTEMA_ESPERA_OBJETO_MS + 200);
    COMPROBAR(sistema_estado() == SIS_IDLE, "tras el tiempo máximo debe volver a IDLE");
    COMPROBAR(visto[SIS_EVT_TIEMPO_AGOTADO] == 1, "falta TIEMPO_AGOTADO");
    COMPROBAR(mock_tcs_llamadas == 0, "no debe haber medido nada");
    /* lejos → ACERCAR; luego correcto y estable → mide */
    memset(visto, 0, sizeof visto);
    color(0, 0, 0);
    sistema_calibrar(true);
    prox(PROX_LEJOS);  avanzar(200);
    COMPROBAR(sistema_guia() == SIS_GUIA_ACERCAR, "objeto lejos: guía ACERCAR (%d)", sistema_guia());
    prox(PROX_CERCA);  avanzar(200);
    COMPROBAR(sistema_guia() == SIS_GUIA_ALEJAR, "objeto muy cerca: guía ALEJAR (%d)", sistema_guia());
    COMPROBAR(visto[SIS_EVT_GUIA] >= 3, "cada cambio de guía debe emitir un evento (%d)", visto[SIS_EVT_GUIA]);
    prox(PROX_OK);
    avanzar(SISTEMA_PROX_ESTABLE_MS / 2);
    COMPROBAR(sistema_estado() == SIS_CALIBRATE, "aún no debe medir: la distancia no lleva el tiempo estable");
    avanzar(SISTEMA_PROX_ESTABLE_MS + 100);
    COMPROBAR(sistema_estado() == SIS_IDLE && visto[SIS_EVT_CAL_NEGRO] == 1, "debe calibrar el negro (estado %s)", sistema_estado_texto(sistema_estado()));
    /* una colocación inestable (entra y sale) no debe medir */
    color(255, 255, 255);
    sistema_calibrar(false);
    int llamadas = mock_tcs_llamadas;
    for (int i = 0; i < 6; i++) { prox(PROX_OK); avanzar(120); prox(PROX_LEJOS); avanzar(120); }
    COMPROBAR(mock_tcs_llamadas == llamadas, "entrar y salir de la distancia correcta no debe medir");
    prox(PROX_OK); avanzar(SISTEMA_PROX_ESTABLE_MS + 150);
    COMPROBAR(visto[SIS_EVT_CAL_BLANCO] == 1, "debe calibrar el blanco");

    TITULO("5. Medir un color y llevar el cursor");
    COMPROBAR(sistema_medir(), "medir con el color calibrado debe aceptarse (rechazo %d)", sistema_rechazo());
    color(40, 200, 60);                                /* verde: H ≈ 127° → ≈ 123 mm */
    avanzar(SISTEMA_PROX_ESTABLE_MS + 150);
    COMPROBAR(visto[SIS_EVT_MEDIDA] == 1, "falta el evento MEDIDA");
    COMPROBAR(sistema_hay_lectura(), "debe haber lectura");
    COMPROBAR(sistema_estado() == SIS_MOVE_TO_TARGET, "debe pasar a MOVE_TO_TARGET (%s)", sistema_estado_texto(sistema_estado()));
    COMPROBAR(sistema_apuntado() == DET_MOVIENDO, "el cursor debe ir hacia el color");
    int32_t objetivo = sistema_objetivo_um();
    COMPROBAR(objetivo > 110000 && objetivo < 135000, "objetivo del verde fuera de rango (%.1f mm)", objetivo / 1000.0);
    COMPROBAR(!sistema_medir() && sistema_rechazo() == SIS_RECHAZO_OCUPADO, "medir mientras se mueve debe rechazarse");
    hasta_estado(SIS_IDLE, 60000);
    avanzar(500);
    COMPROBAR(visto[SIS_EVT_LLEGADA] == 1, "falta LLEGADA");
    COMPROBAR(fabs(planta_x_um - objetivo) < 6000.0, "el cursor quedó a %.1f mm del objetivo %.1f mm",
              fabs(planta_x_um - objetivo) / 1000, objetivo / 1000.0);
    COMPROBAR(!choque, "sin choque");
    printf("   objetivo %.1f mm, cursor real %.1f mm\n", objetivo / 1000.0, planta_x_um / 1000);

    TITULO("6. Colores sin tono: el cursor no se mueve");
    double antes = planta_x_um;
    memset(visto, 0, sizeof visto);
    sistema_medir(); color(250, 250, 250); avanzar(SISTEMA_PROX_ESTABLE_MS + 150);
    COMPROBAR(visto[SIS_EVT_ACROMATICO] == 1, "blanco: falta el evento ACROMATICO");
    COMPROBAR(sistema_estado() == SIS_IDLE, "blanco: debe volver a IDLE");
    COMPROBAR(fabs(planta_x_um - antes) < 100.0, "blanco: el cursor no debe moverse");
    memset(visto, 0, sizeof visto);
    sistema_medir(); color(10, 10, 10); avanzar(SISTEMA_PROX_ESTABLE_MS + 150);
    COMPROBAR(visto[SIS_EVT_ACROMATICO] == 1, "negro: falta el evento ACROMATICO");
    memset(visto, 0, sizeof visto);
    sistema_medir(); color(120, 120, 120); avanzar(SISTEMA_PROX_ESTABLE_MS + 150);
    COMPROBAR(visto[SIS_EVT_ACROMATICO] == 1, "gris: falta el evento ACROMATICO");

    TITULO("7. Lectura inválida (desborde del sensor)");
    memset(visto, 0, sizeof visto);
    mock_tcs_desborde = 1;
    sistema_medir(); avanzar(SISTEMA_PROX_ESTABLE_MS + 150);
    COMPROBAR(visto[SIS_EVT_LECTURA_INVALIDA] == 1, "falta LECTURA_INVALIDA");
    COMPROBAR(sistema_estado() == SIS_IDLE, "debe volver a IDLE tras una lectura inválida");
    mock_tcs_desborde = 0;

    TITULO("8. Cancelar");
    memset(visto, 0, sizeof visto);
    prox(PROX_SIN_OBJETO);
    sistema_medir(); avanzar(300);
    sistema_cancelar();
    avanzar(1);                                       /* el arnés recoge los eventos al avanzar */
    COMPROBAR(sistema_estado() == SIS_IDLE && visto[SIS_EVT_CANCELADO] == 1, "cancelar la espera debe volver a IDLE con evento");
    /* azul (H ≈ 231°, ≈ 224 mm): lejos de donde quedó el cursor, así que sí debe moverse */
    prox(PROX_OK); color(30, 60, 220);
    sistema_medir(); avanzar(SISTEMA_PROX_ESTABLE_MS + 150);
    COMPROBAR(sistema_estado() == SIS_MOVE_TO_TARGET, "debe estar moviéndose (%s)", sistema_estado_texto(sistema_estado()));
    sistema_cancelar();
    avanzar(800);
    COMPROBAR(sistema_estado() == SIS_IDLE, "cancelar el movimiento debe volver a IDLE");
    COMPROBAR(carro_velocidad_real() == 0, "el motor debe quedar parado");

    TITULO("9. Historial");
    sistema_medir(); color(40, 200, 60); avanzar(SISTEMA_PROX_ESTABLE_MS + 150); hasta_estado(SIS_IDLE, 60000);
    COMPROBAR(sistema_guardar(), "guardar con lectura debe funcionar");
    COMPROBAR(historial_cantidad(sistema_historial()) == 1, "el historial debe tener 1 entrada (%d)", historial_cantidad(sistema_historial()));

    TITULO("10. Modo manual");
    sistema_armar(true);
    COMPROBAR(sistema_manual(true) && sistema_estado() == SIS_MANUAL, "debe entrar en MANUAL");
    COMPROBAR(!sistema_medir() && sistema_rechazo() == SIS_RECHAZO_OCUPADO, "medir en MANUAL debe rechazarse");
    sistema_manual_velocidad(40);
    avanzar(1500);
    COMPROBAR(planta_x_um > 130000.0, "el jog debe mover el cursor (%.1f mm)", planta_x_um / 1000);
    sistema_manual_velocidad(40);
    int n = 0;
    while (planta_x_um < LARGO - 2000 && n++ < 30000) avanzar(1);
    avanzar(800);
    COMPROBAR(!choque, "el jog hasta el final no debe chocar (%.1f mm)", planta_x_um / 1000);
    COMPROBAR(planta_x_um > 345000.0, "debe llegar al final (%.1f mm)", planta_x_um / 1000);
    sistema_manual(false);
    COMPROBAR(sistema_estado() == SIS_IDLE, "salir de MANUAL debe volver a IDLE");

    TITULO("11. Desarmar durante una operación");
    reiniciar_todo(175000);
    sistema_armar(true); sistema_referenciar(); avanzar(2000);
    COMPROBAR(sistema_estado() == SIS_HOMING, "debe estar en HOMING");
    sistema_armar(false);
    avanzar(500);
    COMPROBAR(sistema_estado() == SIS_IDLE, "desarmar durante el homing debe volver a IDLE (%s)", sistema_estado_texto(sistema_estado()));
    double x_parado = planta_x_um; avanzar(1000);
    COMPROBAR(fabs(planta_x_um - x_parado) < 200.0, "el carrito debe quedar quieto tras desarmar");

    TITULO("12. Errores y recuperación");
    reiniciar_todo(175000);
    fin_roto = 1;                                     /* el final FIN no se activa nunca */
    armar_y_referenciar();
    COMPROBAR(sistema_estado() == SIS_ERROR, "con un final estropeado debe acabar en ERROR (%s)", sistema_estado_texto(sistema_estado()));
    COMPROBAR(visto[SIS_EVT_ERROR] >= 1, "falta el evento ERROR");
    COMPROBAR(!sistema_medir() && sistema_rechazo() == SIS_RECHAZO_ERROR_ACTIVO, "en ERROR las órdenes se rechazan");
    COMPROBAR(!sistema_armar(true), "en ERROR no se puede armar");
    fin_roto = 0; planta_inicio(175000);
    COMPROBAR(sistema_reiniciar_error(), "reiniciar el error debe funcionar al desaparecer la causa");
    avanzar(10);
    COMPROBAR(sistema_estado() == SIS_IDLE && visto[SIS_EVT_ERROR_REINICIADO] == 1, "debe volver a IDLE (%s)", sistema_estado_texto(sistema_estado()));
    COMPROBAR(!sistema_referenciado(), "tras un error hay que referenciar de nuevo");
    sistema_armar(true);
    COMPROBAR(sistema_referenciar(), "tras reiniciar se puede referenciar");
    hasta_estado(SIS_IDLE, 80000);
    COMPROBAR(sistema_referenciado(), "y el eje vuelve a quedar calibrado");

    TITULO("13. Arranque con los dos finales activos a la vez (fallo de cableado)");
    memset(visto, 0, sizeof visto);
    planta_inicio(175000);
    forzar_ini = forzar_fin = 1;
    prox(PROX_SIN_OBJETO);
    sistema_iniciar();
    avanzar(20);
    COMPROBAR(sistema_estado() == SIS_ERROR, "con los dos finales activos debe arrancar en ERROR (%s)", sistema_estado_texto(sistema_estado()));
    COMPROBAR(visto[SIS_EVT_ERROR] == 1 && visto[SIS_EVT_LISTO] == 0, "debe emitir ERROR y no LISTO");
    COMPROBAR(!sistema_armar(true), "no se puede armar con ese error");
    COMPROBAR(!sistema_reiniciar_error(), "mientras la causa persista, el error no se reinicia");
    forzar_ini = forzar_fin = 0;
    avanzar(20);
    COMPROBAR(sistema_reiniciar_error(), "al corregir el cableado, el error se reinicia");
    COMPROBAR(sistema_estado() == SIS_IDLE, "y vuelve a IDLE");

    printf("\n==========  %d comprobaciones correctas, %d fallos  ==========\n", pasa, falla);
    return falla ? 1 : 0;
}
