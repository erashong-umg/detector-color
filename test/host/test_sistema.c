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
#include "configuracion/escala.h"
#include "configuracion/proximidad.h"
#include "configuracion/sistema.h"
#include "dominio/proximidad.h"
#include "dominio/selector.h"
#include "drivers/finales.h"
#include "drivers/tcs3200.h"
#include "hw/tiempo.h"

uint32_t mock_us;
extern int planta_dir;
extern double planta_duty;
extern int mock_adc_prox, mock_od_nivel;
extern tcs_frecuencias_t mock_frecuencias;
extern int mock_tcs_desborde, mock_tcs_llamadas, mock_tcs_async_activo;

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
    K = LARGO / ((double)EJE_RECORRIDO_ESPERADO_MS) / ((double)duty40 - ZONA_MUERTA);
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

#ifdef SOLO_PROTOCOLO
#include "aplicacion/protocolo.h"

/* Las líneas que el protocolo envía, para comprobarlas. */
static char salida_buf[64][240];
static int  salida_n;

static void capturar(const char *linea)
{
    if (salida_n < 64) {
        snprintf(salida_buf[salida_n++], sizeof salida_buf[0], "%s", linea);
    }
}

static void limpiar_salida(void) { salida_n = 0; }

static int hubo(const char *trozo)
{
    for (int i = 0; i < salida_n; i++) {
        if (strstr(salida_buf[i], trozo)) {
            return 1;
        }
    }
    return 0;
}
#endif

/* La adquisición por ISR ya no bloquea: DETECCION_MUESTRAS juegos de 88 ms van pasando en el bucle. */
#define MEDIDA_MS  (DETECCION_MUESTRAS * 88 + 40)

static void avanzar(int ms)
{
    for (int i = 0; i < ms; i++) {
        sistema_actualizar();
        planta_paso();
        mock_us += 1000;
        for (sistema_evento_t e; (e = sistema_tomar_evento()) != SIS_EVT_NINGUNO; ) {
            visto[e]++;
#ifdef SOLO_PROTOCOLO
            protocolo_evento(e);
#endif
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
    case PROX_CERCA:      nivel = 4095; od_activo = 1; break;
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
#ifdef SOLO_PROTOCOLO
    TITULO("Protocolo serie: comandos, respuestas y eventos");
    reiniciar_todo(175000);
    protocolo_iniciar(capturar);
    COMPROBAR(hubo("HOLA,detector-color,protocolo=1"), "debe saludar al iniciar");

    limpiar_salida(); protocolo_procesar_linea("AYUDA");
    COMPROBAR(hubo("AYUDA,"), "AYUDA debe listar los comandos");
    limpiar_salida(); protocolo_procesar_linea("estado");
    COMPROBAR(hubo("ESTADO,estado=IDLE,armado=0,referenciado=0,calibrado=0"), "ESTADO inicial (sin distinguir mayúsculas)");
    limpiar_salida(); protocolo_procesar_linea("LECTURA");
    COMPROBAR(hubo("ERR,SIN_LECTURA"), "LECTURA sin medir debe dar error");
    limpiar_salida(); protocolo_procesar_linea("MEDIR");
    COMPROBAR(hubo("ERR,NO_CALIBRADO"), "MEDIR sin calibrar el color");
    limpiar_salida(); protocolo_procesar_linea("IR 160");
    COMPROBAR(hubo("ERR,NO_ARMADO"), "IR sin armar");
    limpiar_salida(); protocolo_procesar_linea("COSA");
    COMPROBAR(hubo("ERR,COMANDO"), "comando desconocido");
    limpiar_salida(); protocolo_procesar_linea("CAL");
    COMPROBAR(hubo("ERR,ARGUMENTO"), "CAL sin argumento");
    limpiar_salida(); protocolo_procesar_linea("IR abc");
    COMPROBAR(hubo("ERR,ARGUMENTO"), "IR con un argumento no numérico");
    limpiar_salida(); protocolo_procesar_linea("IR 9999");
    COMPROBAR(hubo("ERR,FUERA_DE_RANGO"), "IR fuera del recorrido");
    limpiar_salida(); protocolo_procesar_linea("   ");
    COMPROBAR(salida_n == 0, "una línea vacía no responde nada");

    /* Entrada byte a byte, con CRLF. */
    limpiar_salida();
    { const char *s = "estado\r\n"; for (; *s; s++) protocolo_byte(*s); }
    COMPROBAR(salida_n == 1 && hubo("ESTADO,"), "byte a byte con CRLF: una sola respuesta (%d)", salida_n);
    limpiar_salida();
    for (int i = 0; i < 100; i++) protocolo_byte('A');
    protocolo_byte('\n');
    COMPROBAR(hubo("ERR,LINEA_LARGA"), "una línea demasiado larga debe rechazarse");

    /* Armar, referenciar y ver los eventos. */
    limpiar_salida(); protocolo_procesar_linea("ARMAR");
    COMPROBAR(hubo("OK"), "ARMAR");
    limpiar_salida(); protocolo_procesar_linea("REF");
    COMPROBAR(hubo("OK"), "REF");
    hasta_estado(SIS_IDLE, 80000);
    COMPROBAR(hubo("EVT,REFERENCIADO"), "debe enviar EVT,REFERENCIADO");
    limpiar_salida(); protocolo_procesar_linea("ESTADO");
    COMPROBAR(hubo("armado=1,referenciado=1"), "ESTADO tras referenciar");

    /* Calibrar. */
    prox(PROX_OK);
    color(0, 0, 0);
    limpiar_salida(); protocolo_procesar_linea("CAL NEGRO");
    COMPROBAR(hubo("OK"), "CAL NEGRO");
    avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
    COMPROBAR(hubo("EVT,ANALIZANDO") && hubo("EVT,CAL_NEGRO"), "eventos de calibrar el negro");
    color(255, 255, 255);
    limpiar_salida(); protocolo_procesar_linea("CAL BLANCO");
    avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
    COMPROBAR(hubo("EVT,CAL_BLANCO"), "evento de calibrar el blanco");

    /* Medir un verde (frío) y ver la lectura. */
    color(40, 200, 60);
    limpiar_salida(); protocolo_procesar_linea("MEDIR");
    COMPROBAR(hubo("OK"), "MEDIR");
    avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
    COMPROBAR(hubo("EVT,MEDIDA,") && hubo("temp=FRIO") && hubo("clase=CROMATICO"), "EVT,MEDIDA con temperatura FRIO");
    COMPROBAR(hubo("obj_mm=266.7"), "el objetivo del frío debe ser 266.7 mm");
    limpiar_salida(); protocolo_procesar_linea("LECTURA");
    COMPROBAR(hubo("LECTURA,hex=#") && hubo("hz="), "LECTURA con el color y las frecuencias");
    hasta_estado(SIS_IDLE, 60000);
    COMPROBAR(hubo("EVT,LLEGADA"), "debe enviar EVT,LLEGADA");

    /* Historial. */
    limpiar_salida(); protocolo_procesar_linea("GUARDAR");
    COMPROBAR(hubo("OK"), "GUARDAR");
    limpiar_salida(); protocolo_procesar_linea("HIST");
    COMPROBAR(hubo("HIST,0,hex=#") && hubo("HIST,FIN,1"), "HIST con una entrada");

    /* Mover y parar. */
    limpiar_salida(); protocolo_procesar_linea("IR 100.5");
    COMPROBAR(hubo("OK"), "IR con decimales");
    avanzar(300);
    limpiar_salida(); protocolo_procesar_linea("PARAR");
    COMPROBAR(hubo("OK"), "PARAR");
    avanzar(800);
    limpiar_salida(); protocolo_procesar_linea("DESARMAR");
    COMPROBAR(hubo("OK"), "DESARMAR");

    printf("\n==========  %d comprobaciones correctas, %d fallos  ==========\n", pasa, falla);
    return falla ? 1 : 0;
#endif
#ifdef SOLO_AUTOREF
    TITULO("Autoreferenciar al iniciar");
    reiniciar_todo(175000);
    COMPROBAR(sistema_estado() == SIS_IDLE && visto[SIS_EVT_LISTO] == 1, "tras INIT debe estar en IDLE y haber emitido LISTO");
    COMPROBAR(!sistema_armado(), "antes del retardo no debe haberse armado");
    avanzar(SISTEMA_AUTOREF_RETARDO_MS + 50);
    COMPROBAR(sistema_armado() && sistema_estado() == SIS_HOMING, "pasado el retardo debe armar y empezar el homing (%s)", sistema_estado_texto(sistema_estado()));
    COMPROBAR(visto[SIS_EVT_AUTOREFERENCIANDO] == 1, "falta el evento AUTOREFERENCIANDO");
    int ta = hasta_estado(SIS_IDLE, 80000);
    COMPROBAR(sistema_estado() == SIS_IDLE && sistema_referenciado(), "debe terminar referenciado y en IDLE (%s tras %d ms)", sistema_estado_texto(sistema_estado()), ta);
    COMPROBAR(visto[SIS_EVT_REFERENCIADO] == 1, "falta el evento REFERENCIADO");
    COMPROBAR(planta_x_um < 3000.0 && !choque, "el carrito debe quedar junto al inicio sin choque (%.1f mm)", planta_x_um / 1000);
    avanzar(SISTEMA_AUTOREF_RETARDO_MS + 50);
    COMPROBAR(visto[SIS_EVT_AUTOREFERENCIANDO] == 1, "la referencia automática solo debe ocurrir una vez");

    TITULO("Autoreferenciar: reintentos si falla (final de INICIO roto)");
    reiniciar_todo(175000);
    ini_roto = 1;                       /* el final INICIO no se activa nunca: el homing agota el tiempo */
    memset(visto, 0, sizeof visto);
    avanzar(SISTEMA_AUTOREF_RETARDO_MS + 20000);
    COMPROBAR(visto[SIS_EVT_AUTOREF_REINTENTO] == SISTEMA_AUTOREF_INTENTOS,
              "debe reintentar %d veces (%d)", SISTEMA_AUTOREF_INTENTOS, visto[SIS_EVT_AUTOREF_REINTENTO]);
    COMPROBAR(sistema_estado() == SIS_ERROR, "agotados los reintentos debe quedar en ERROR (%s)", sistema_estado_texto(sistema_estado()));
    COMPROBAR(visto[SIS_EVT_ERROR] == 1, "y avisar del error una sola vez (%d)", visto[SIS_EVT_ERROR]);
    COMPROBAR(sistema_autoref_intentos() == SISTEMA_AUTOREF_INTENTOS, "el contador de intentos debe llegar a %d", SISTEMA_AUTOREF_INTENTOS);
    ini_roto = 0;

    TITULO("Autoreferenciar: no con un error al arrancar");
    memset(visto, 0, sizeof visto);
    planta_inicio(175000);
    forzar_ini = forzar_fin = 1;
    prox(PROX_SIN_OBJETO);
    sistema_iniciar();
    avanzar(SISTEMA_AUTOREF_RETARDO_MS + 50);
    COMPROBAR(sistema_estado() == SIS_ERROR && !sistema_armado(), "con los dos finales activos no debe armar ni moverse (%s)", sistema_estado_texto(sistema_estado()));
    COMPROBAR(visto[SIS_EVT_AUTOREFERENCIANDO] == 0, "no debe emitir AUTOREFERENCIANDO");

    /* Al reconectar los finales el sistema se recupera solo y vuelve a referenciar. */
    forzar_ini = forzar_fin = 0;
    avanzar(SISTEMA_RECUPERAR_MS - 100);
    COMPROBAR(sistema_estado() == SIS_ERROR && visto[SIS_EVT_RECUPERADO] == 0,
              "antes de SISTEMA_RECUPERAR_MS debe seguir en ERROR (%s)", sistema_estado_texto(sistema_estado()));
    avanzar(300);
    COMPROBAR(visto[SIS_EVT_RECUPERADO] == 1, "debe emitir RECUPERADO al volver los finales coherentes");
    avanzar(SISTEMA_AUTOREF_REINTENTO_MS + 100);
    COMPROBAR(visto[SIS_EVT_AUTOREFERENCIANDO] == 1 && sistema_armado(), "y volver a referenciar solo");
    hasta_estado(SIS_IDLE, 80000);
    COMPROBAR(sistema_referenciado(), "debe terminar referenciado (%s)", sistema_estado_texto(sistema_estado()));

    /* Un cable intermitente no debe recuperar el sistema: cada corte reinicia la cuenta. */
    TITULO("Autoreferenciar: finales intermitentes no recuperan");
    memset(visto, 0, sizeof visto);
    planta_inicio(175000);
    forzar_ini = forzar_fin = 1;
    prox(PROX_SIN_OBJETO);
    sistema_iniciar();
    avanzar(50);
    COMPROBAR(sistema_estado() == SIS_ERROR, "con los dos finales activos arranca en ERROR");
    for (int i = 0; i < 6; i++) {
        forzar_ini = forzar_fin = 0; avanzar(SISTEMA_RECUPERAR_MS / 2);
        forzar_ini = forzar_fin = 1; avanzar(50);
    }
    COMPROBAR(sistema_estado() == SIS_ERROR && visto[SIS_EVT_RECUPERADO] == 0, "intermitente: debe seguir en ERROR sin recuperarse");
    forzar_ini = forzar_fin = 0;

    /* Sensor de color desconectado: sin señal no se calibra ni se mide. */
    TITULO("Sensor de color sin señal");
    reiniciar_todo(175000);
    avanzar(SISTEMA_AUTOREF_RETARDO_MS + 20);
    hasta_estado(SIS_IDLE, 80000);
    memset(visto, 0, sizeof visto);
    mock_frecuencias.r = mock_frecuencias.g = mock_frecuencias.b = mock_frecuencias.c = 0;
    prox(PROX_OK);
    COMPROBAR(sistema_calibrar(true), "calibrar negro se acepta");
    avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
    COMPROBAR(visto[SIS_EVT_LECTURA_INVALIDA] == 1 && !deteccion_hay_negro(), "sin señal: lectura inválida y sin guardar la calibración");

    printf("\n==========  %d comprobaciones correctas, %d fallos  ==========\n", pasa, falla);
    return falla ? 1 : 0;
#endif

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
    avanzar(SISTEMA_PROX_ESTABLE_MS + 100 + MEDIDA_MS);
    COMPROBAR(sistema_estado() == SIS_IDLE && visto[SIS_EVT_CAL_NEGRO] == 1, "debe calibrar el negro (estado %s)", sistema_estado_texto(sistema_estado()));
    /* una colocación inestable (entra y sale) no debe medir */
    color(255, 255, 255);
    sistema_calibrar(false);
    int llamadas = mock_tcs_llamadas;
    for (int i = 0; i < 6; i++) { prox(PROX_OK); avanzar(120); prox(PROX_LEJOS); avanzar(120); }
    COMPROBAR(mock_tcs_llamadas == llamadas, "entrar y salir de la distancia correcta no debe medir");
    prox(PROX_OK); avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
    COMPROBAR(visto[SIS_EVT_CAL_BLANCO] == 1, "debe calibrar el blanco");
    /* B1 largo: con una muestra oscura el IR no la ve; forzar ejecuta sin esperar a PROX_OK */
    sistema_calibrar(false);
    prox(PROX_SIN_OBJETO); avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
    COMPROBAR(sistema_estado() == SIS_CALIBRATE, "sin objeto y sin forzar debe seguir esperando");
    sistema_forzar_objeto(); avanzar(100 + MEDIDA_MS);
    COMPROBAR(sistema_estado() == SIS_IDLE && visto[SIS_EVT_CAL_BLANCO] == 2, "forzado debe calibrar sin esperar al IR (estado %s)", sistema_estado_texto(sistema_estado()));
    sistema_forzar_objeto();
    COMPROBAR(sistema_estado() == SIS_IDLE, "forzar fuera de MEASURE/CALIBRATE no debe hacer nada");
    prox(PROX_OK); avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);           /* el resto de pruebas parte con la muestra en su sitio */

    TITULO("5. Medir un color frío y llevar el carrito");
    COMPROBAR(sistema_medir(), "medir con el color calibrado debe aceptarse (rechazo %d)", sistema_rechazo());
    color(40, 200, 60);                                /* verde: H ≈ 127° → frío */
    avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
    COMPROBAR(visto[SIS_EVT_MEDIDA] == 1, "falta el evento MEDIDA");
    COMPROBAR(sistema_hay_lectura(), "debe haber lectura");
    COMPROBAR(sistema_lectura()->escala.temperatura == TEMP_FRIO, "el verde debe ser frío");
    COMPROBAR(sistema_estado() == SIS_MOVE_TO_TARGET, "debe pasar a MOVE_TO_TARGET (%s)", sistema_estado_texto(sistema_estado()));
    COMPROBAR(sistema_apuntado() == DET_MOVIENDO, "el carrito debe ir hacia la posición fría");
    int32_t objetivo = sistema_objetivo_um();
    COMPROBAR(objetivo == ESCALA_POS_FRIO_UM, "objetivo del verde: %.1f mm, esperado %.1f mm", objetivo / 1000.0, ESCALA_POS_FRIO_UM / 1000.0);
    COMPROBAR(!sistema_medir() && sistema_rechazo() == SIS_RECHAZO_OCUPADO, "medir mientras se mueve debe rechazarse");
    hasta_estado(SIS_IDLE, 60000);
    avanzar(500);
    COMPROBAR(visto[SIS_EVT_LLEGADA] == 1, "falta LLEGADA");
    COMPROBAR(fabs(planta_x_um - objetivo) < 6000.0, "el carrito quedó a %.1f mm del objetivo %.1f mm",
              fabs(planta_x_um - objetivo) / 1000, objetivo / 1000.0);
    COMPROBAR(!choque, "sin choque");
    printf("   objetivo %.1f mm, carrito real %.1f mm\n", objetivo / 1000.0, planta_x_um / 1000);

    TITULO("6. Cálido y neutro (colores sin tono)");
    memset(visto, 0, sizeof visto);
    sistema_medir(); color(230, 40, 40); avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
    COMPROBAR(sistema_lectura()->escala.temperatura == TEMP_CALIDO, "el rojo debe ser cálido");
    COMPROBAR(sistema_objetivo_um() == ESCALA_POS_CALIDO_UM, "rojo: objetivo %.1f mm", sistema_objetivo_um() / 1000.0);
    COMPROBAR(sistema_estado() == SIS_MOVE_TO_TARGET, "rojo: debe moverse a la posición cálida");
    hasta_estado(SIS_IDLE, 60000); avanzar(500);
    COMPROBAR(fabs(planta_x_um - ESCALA_POS_CALIDO_UM) < 15000.0, "rojo: carrito en %.1f mm", planta_x_um / 1000);
    memset(visto, 0, sizeof visto);
    sistema_medir(); color(250, 250, 250); avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
    COMPROBAR(sistema_lectura()->escala.temperatura == TEMP_NEUTRO, "blanco: debe ser neutro");
    COMPROBAR(sistema_objetivo_um() == ESCALA_POS_NEUTRO_UM, "blanco: objetivo %.1f mm", sistema_objetivo_um() / 1000.0);
    COMPROBAR(sistema_estado() == SIS_MOVE_TO_TARGET, "blanco: debe moverse a la posición neutra");
    hasta_estado(SIS_IDLE, 60000); avanzar(500);
    COMPROBAR(fabs(planta_x_um - ESCALA_POS_NEUTRO_UM) < 15000.0, "blanco: carrito en %.1f mm", planta_x_um / 1000);
    memset(visto, 0, sizeof visto);
    sistema_medir(); color(10, 10, 10); avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
    COMPROBAR(sistema_lectura()->escala.temperatura == TEMP_NEUTRO && visto[SIS_EVT_MEDIDA] == 1, "negro: neutro y evento MEDIDA");
    COMPROBAR(sistema_estado() == SIS_IDLE, "negro: ya está en la posición neutra, no se mueve");
    memset(visto, 0, sizeof visto);
    sistema_medir(); color(120, 120, 120); avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
    COMPROBAR(sistema_lectura()->escala.temperatura == TEMP_NEUTRO && visto[SIS_EVT_MEDIDA] == 1, "gris: neutro y evento MEDIDA");

    TITULO("7. Lectura inválida (desborde del sensor)");
    memset(visto, 0, sizeof visto);
    mock_tcs_desborde = 1;
    sistema_medir(); avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
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
    /* azul (frío): lejos de la posición neutra donde quedó el carrito, así que sí debe moverse */
    prox(PROX_OK); color(30, 60, 220);
    sistema_medir(); avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS);
    COMPROBAR(sistema_estado() == SIS_MOVE_TO_TARGET, "debe estar moviéndose (%s)", sistema_estado_texto(sistema_estado()));
    sistema_cancelar();
    avanzar(800);
    COMPROBAR(sistema_estado() == SIS_IDLE, "cancelar el movimiento debe volver a IDLE");
    COMPROBAR(carro_velocidad_real() == 0, "el motor debe quedar parado");
    /* cancelar a mitad de la adquisición por ISR: debe detenerla y no producir ninguna medida */
    memset(visto, 0, sizeof visto);
    prox(PROX_OK); color(30, 60, 220);
    sistema_medir();
    for (int i = 0; i < 3000 && !mock_tcs_async_activo; i++) avanzar(1);
    COMPROBAR(mock_tcs_async_activo, "la ISR debe estar adquiriendo tras esperar al objeto");
    avanzar(100);
    COMPROBAR(sistema_estado() == SIS_MEASURE && visto[SIS_EVT_MEDIDA] == 0, "a mitad de la adquisición aún no hay medida");
    sistema_cancelar();
    avanzar(1);
    COMPROBAR(!mock_tcs_async_activo, "cancelar debe detener la ISR");
    COMPROBAR(sistema_estado() == SIS_IDLE && visto[SIS_EVT_CANCELADO] == 1, "cancelar la medida debe volver a IDLE con evento");
    avanzar(500);
    COMPROBAR(visto[SIS_EVT_MEDIDA] == 0 && !mock_tcs_async_activo, "no debe llegar ninguna medida después de cancelar");

    TITULO("9. Historial");
    sistema_medir(); color(40, 200, 60); avanzar(SISTEMA_PROX_ESTABLE_MS + 150 + MEDIDA_MS); hasta_estado(SIS_IDLE, 60000);
    COMPROBAR(sistema_guardar(), "guardar con lectura debe funcionar");
    COMPROBAR(historial_cantidad(sistema_historial()) == 1, "el historial debe tener 1 entrada (%d)", historial_cantidad(sistema_historial()));

    TITULO("9b. Selector de color del modo manual (marca en la barra cromática)");
    {
        selector_t sel;
        selector_iniciar(&sel);
        COMPROBAR(escala_mapear(selector_hsv(&sel)).temperatura == TEMP_CALIDO, "arranca en rojo: cálido");
        for (int i = 0; i < 100; i++) selector_mover(&sel, 100, 0);      /* a tope: 1,2° por llamada → 120° */
        COMPROBAR(selector_hsv(&sel).h == 120 && escala_mapear(selector_hsv(&sel)).temperatura == TEMP_FRIO,
                  "a 120° (verde) debe ser frío (h=%d)", selector_hsv(&sel).h);
        for (int i = 0; i < 100; i++) selector_mover(&sel, 100, 0);
        COMPROBAR(selector_hsv(&sel).h == 240 && escala_mapear(selector_hsv(&sel)).temperatura == TEMP_FRIO, "a 240° (azul) debe ser frío");
        for (int i = 0; i < 100; i++) selector_mover(&sel, 100, 0);
        COMPROBAR(selector_hsv(&sel).h == 0 && escala_mapear(selector_hsv(&sel)).temperatura == TEMP_CALIDO,
                  "da la vuelta a 360° y vuelve al rojo, cálido (h=%d)", selector_hsv(&sel).h);
        selector_mover(&sel, -100, 0);
        COMPROBAR(selector_hsv(&sel).h == 358, "hacia la izquierda desde 0° da la vuelta (h=%d)", selector_hsv(&sel).h);
        selector_mover(&sel, 0, +1);                                    /* abajo: menos saturación */
        COMPROBAR(sel.s == 255 - SELECTOR_PASO_SATURACION, "abajo baja la saturación (%d)", sel.s);
        for (int i = 0; i < 10; i++) selector_mover(&sel, 0, +1);
        COMPROBAR(sel.s == 0 && escala_mapear(selector_hsv(&sel)).temperatura == TEMP_NEUTRO, "sin saturación es un gris: neutro");
        for (int i = 0; i < 20; i++) selector_mover(&sel, 0, -1);
        COMPROBAR(sel.s == 255, "arriba sube la saturación hasta 255 sin pasarse (%d)", sel.s);
    }

    TITULO("10. Modo manual");
    sistema_armar(true);
    COMPROBAR(sistema_manual(true) && sistema_estado() == SIS_MANUAL, "debe entrar en MANUAL");
    COMPROBAR(sistema_manual_ir_a(ESCALA_POS_FRIO_UM), "dentro de MANUAL se puede ir a una posición");
    avanzar(2500);
    COMPROBAR(sistema_estado() == SIS_MANUAL && fabs(planta_x_um - ESCALA_POS_FRIO_UM) < 40000.0,
              "el carrito debe ir a la posición fría sin salir de MANUAL (%.1f mm, %s)", planta_x_um / 1000, sistema_estado_texto(sistema_estado()));
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
    COMPROBAR(!sistema_manual_ir_a(100000), "fuera de MANUAL sistema_manual_ir_a debe rechazarse");

    TITULO("11. Desarmar durante una operación");
    reiniciar_todo(175000);
    sistema_armar(true); sistema_referenciar(); avanzar(300);
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
