/*
 * app.c — Aplicación del detector de color.
 *
 * Controles (ver también ui/interfaz.h):
 *   Principal:  B1 medir · B2 menú de calibración · B3 mover cursor ·
 *               B4 corto guarda la lectura · B4 largo o SW abre el menú
 *   Operación en curso (HOMING, MEASURE, CALIBRATE, MOVE_TO_TARGET):
 *               B4 o SW la cancelan
 *   MANUAL:     joystick X mueve el cursor · B4 o SW salen
 *   ERROR:      B4 largo reinicia el error
 */
#include "aplicacion/app.h"

#include <stdbool.h>
#include <stdio.h>

#include "aplicacion/carro.h"
#include "aplicacion/deteccion.h"
#include "aplicacion/eje.h"
#include "aplicacion/sistema.h"
#include "configuracion/motor.h"
#include "drivers/adc.h"
#include "drivers/botones.h"
#include "drivers/buzzer.h"
#include "drivers/finales.h"
#include "drivers/joystick.h"
#include "drivers/led.h"
#include "drivers/proximidad.h"
#include "drivers/st7789.h"
#include "dominio/sonidos.h"
#include "hw/tiempo.h"
#include "ui/interfaz.h"

#define C_OK_UI      ST7789_RGB565(70, 200, 120)
#define C_AVISO_UI   ST7789_RGB565(240, 175, 50)
#define C_ERROR_UI   ST7789_RGB565(235, 75, 75)
#define C_TENUE_UI   ST7789_RGB565(135, 140, 152)
#define C_ACENTO_UI  ST7789_RGB565(80, 155, 245)

#define MENSAJE_MS         3000u
#define JOY_UMBRAL_DIR     55
#define JOY_REPETICION_US  220000u
#define PERIODO_ENTRADA_US 10000u
#define PERIODO_DIBUJO_US  8000u

static ui_modelo_t modelo;
static uint32_t    mensaje_hasta;

static void mensaje(const char *texto)
{
    snprintf(modelo.mensaje, sizeof modelo.mensaje, "%s", texto);
    mensaje_hasta = tiempo_us() + MENSAJE_MS * 1000u;
}

/* ------------------------------------------------------------------ */
/* Textos para el usuario                                              */
/* ------------------------------------------------------------------ */

/* La línea de mensajes tiene 38 caracteres. */
static const char *texto_rechazo(sistema_rechazo_t r)
{
    switch (r) {
    case SIS_RECHAZO_OCUPADO:        return "Espera: hay una operación en curso";
    case SIS_RECHAZO_ERROR_ACTIVO:   return "Hay un error: B4 largo lo reinicia";
    case SIS_RECHAZO_NO_ARMADO:      return "Arma el motor primero";
    case SIS_RECHAZO_SIN_REFERENCIA: return "Referencia el eje primero";
    case SIS_RECHAZO_NO_CALIBRADO:   return "Falta calibrar negro y blanco";
    case SIS_RECHAZO_NINGUNO:        break;
    }
    return "Orden rechazada";
}

static const char *texto_guia(sistema_guia_t g)
{
    switch (g) {
    case SIS_GUIA_COLOCAR_OBJETO: return "No hay objeto: colócalo en el sensor";
    case SIS_GUIA_ACERCAR:        return "Acerca el objeto al sensor";
    case SIS_GUIA_ALEJAR:         return "Aleja un poco el objeto";
    case SIS_GUIA_NINGUNA:        break;
    }
    return "Objeto a buena distancia: no lo muevas";
}

static const char *error_corto(void)
{
    switch (carro_error()) {
    case CARRO_ERROR_FINALES_INCOHERENTES: return "Los dos finales activos a la vez";
    case CARRO_ERROR_TIMEOUT:              return "Tiempo máximo de movimiento";
    case CARRO_ERROR_NINGUNO:              break;
    }
    switch (eje_error()) {
    case EJE_ERROR_SENTIDO_INVERTIDO:      return "Sentido del motor invertido";
    case EJE_ERROR_RECORRIDO_CORTO:        return "Calibración demasiado corta";
    default:                               return "";
    }
}

static const char *texto_apuntado(det_resultado_t r)
{
    switch (r) {
    case DET_MOVIENDO:       return "Moviendo el cursor";
    case DET_YA_APUNTADO:    return "El cursor ya apunta aquí";
    case DET_ACROMATICO:     return "Sin tono: el cursor no se mueve";
    case DET_SIN_REFERENCIA: return "Medido (eje sin referenciar)";
    case DET_NO_PUEDE:       return "Medido (motor desarmado)";
    case DET_NO_VALIDA:      break;
    }
    return "Lectura no válida";
}

/* ------------------------------------------------------------------ */
/* Modelo para la interfaz                                             */
/* ------------------------------------------------------------------ */

static void actualizar_modelo(void)
{
    const char *texto;
    uint16_t color;
    sistema_estado_t est = sistema_estado();

    modelo.armado = sistema_armado();
    modelo.chip_arm = modelo.armado;
    modelo.chip_ref = sistema_referenciado();
    modelo.chip_cal = deteccion_calibrado();
    modelo.chip_wifi = false;           /* el Wi-Fi está reservado, sin usar */

    switch (est) {
    case SIS_ERROR:          texto = "ERROR";      color = C_ERROR_UI;  break;
    case SIS_INIT:           texto = "INICIO";     color = C_TENUE_UI;  break;
    case SIS_HOMING:         texto = eje_estado() == EJE_CALIBRANDO ? "CALIBRANDO" : "HOMING";
                             color = C_AVISO_UI;                        break;
    case SIS_MEASURE:        texto = "MIDIENDO";   color = C_AVISO_UI;  break;
    case SIS_CALIBRATE:      texto = "CAL. COLOR"; color = C_AVISO_UI;  break;
    case SIS_MOVE_TO_TARGET: texto = "MOVIENDO";   color = C_ACENTO_UI; break;
    case SIS_MANUAL:         texto = "MANUAL";     color = C_ACENTO_UI; break;
    case SIS_IDLE:
    default:
        if (!modelo.armado)               { texto = "DESARMADO"; color = C_TENUE_UI; }
        else if (!modelo.chip_ref)        { texto = "SIN REF.";  color = C_AVISO_UI; }
        else                              { texto = "LISTO";     color = C_OK_UI; }
        break;
    }
    snprintf(modelo.estado, sizeof modelo.estado, "%s", texto);
    modelo.estado_color = color;

    modelo.pos_um = eje_posicion_um();
    snprintf(modelo.eje, sizeof modelo.eje, "%s", eje_estado_texto(eje_estado()));
    modelo.ir_inicio = final_inicio_activo();
    modelo.ir_fin = final_fin_activo();
    snprintf(modelo.prox, sizeof modelo.prox, "%s", prox_texto(proximidad_estado()));
    modelo.hay_negro = deteccion_hay_negro();
    modelo.hay_blanco = deteccion_hay_blanco();
    modelo.recorrido_ms = modelo.chip_ref ? eje_recorrido_ms() : 0;
    snprintf(modelo.error, sizeof modelo.error, "%s", est == SIS_ERROR ? error_corto() : "");

    if (sistema_hay_lectura()) {
        const lectura_t *l = sistema_lectura();
        modelo.hay_lectura = true;
        modelo.rgb = l->rgb;
        snprintf(modelo.hex, sizeof modelo.hex, "%s", l->hex);
        modelo.h = l->hsv.h;
        modelo.s = l->hsv.s;
        modelo.v = l->hsv.v;
        modelo.clase = l->escala.clase;
    }
    modelo.objetivo_um = sistema_objetivo_um();
    modelo.historial = sistema_historial();

    /* Mientras se espera al objeto, la guía manda sobre cualquier otro mensaje. */
    if (est == SIS_MEASURE || est == SIS_CALIBRATE) {
        snprintf(modelo.mensaje, sizeof modelo.mensaje, "%s", texto_guia(sistema_guia()));
        mensaje_hasta = tiempo_us() + 500000u;
    } else if (modelo.mensaje[0] && (int32_t)(tiempo_us() - mensaje_hasta) >= 0) {
        modelo.mensaje[0] = '\0';
    }
}

/* ------------------------------------------------------------------ */
/* Eventos del sistema → sonido, LED y mensaje                         */
/* ------------------------------------------------------------------ */

static void atender_eventos(void)
{
    for (sistema_evento_t e = sistema_tomar_evento(); e != SIS_EVT_NINGUNO; e = sistema_tomar_evento()) {
        switch (e) {
        case SIS_EVT_LISTO:
            buzzer_reproducir(&SONIDO_INICIO);
            mensaje("Calibra negro y blanco para empezar");
            break;
        case SIS_EVT_REFERENCIADO:
            buzzer_reproducir(&SONIDO_CONFIRMAR);
            mensaje("Eje referenciado");
            break;
        case SIS_EVT_MEDIDA:
            buzzer_reproducir(&SONIDO_CONFIRMAR);
            led_color(sistema_lectura()->rgb);
            mensaje(texto_apuntado(sistema_apuntado()));
            break;
        case SIS_EVT_ACROMATICO:
            buzzer_reproducir(&SONIDO_ALERTA);
            led_color(sistema_lectura()->rgb);
            mensaje(texto_apuntado(DET_ACROMATICO));
            break;
        case SIS_EVT_CAL_NEGRO:
            buzzer_reproducir(&SONIDO_CONFIRMAR);
            mensaje("Negro calibrado");
            break;
        case SIS_EVT_CAL_BLANCO:
            buzzer_reproducir(&SONIDO_CONFIRMAR);
            mensaje("Blanco calibrado");
            break;
        case SIS_EVT_LLEGADA:
            buzzer_reproducir(&SONIDO_CONFIRMAR);
            mensaje("Cursor en posición");
            break;
        case SIS_EVT_GUIA:
            if (sistema_guia() != SIS_GUIA_NINGUNA) {
                buzzer_reproducir(&SONIDO_ALERTA);
                led_rgb(255, 160, 0);
            } else {
                buzzer_reproducir(&SONIDO_CLIC);
                led_rgb(0, 200, 90);
            }
            break;
        case SIS_EVT_TIEMPO_AGOTADO:
            buzzer_reproducir(&SONIDO_ERROR);
            led_apagar();
            mensaje("Sin objeto: medición cancelada");
            break;
        case SIS_EVT_CANCELADO:
            buzzer_reproducir(&SONIDO_CLIC);
            led_apagar();
            mensaje("Cancelado");
            break;
        case SIS_EVT_LECTURA_INVALIDA:
            buzzer_reproducir(&SONIDO_ERROR);
            mensaje("Lectura no válida (desborde)");
            break;
        case SIS_EVT_ERROR:
            buzzer_reproducir(&SONIDO_ERROR);
            led_rgb(255, 0, 0);
            break;
        case SIS_EVT_ERROR_REINICIADO:
            buzzer_reproducir(&SONIDO_CONFIRMAR);
            led_apagar();
            mensaje("Error reiniciado: referencia el eje");
            break;
        case SIS_EVT_NINGUNO:
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Órdenes de la interfaz                                              */
/* ------------------------------------------------------------------ */

static void rechazada(void)
{
    mensaje(texto_rechazo(sistema_rechazo()));
    buzzer_reproducir(&SONIDO_ALERTA);
}

static void ejecutar(const ui_orden_t *o)
{
    bool ok = true;

    switch (o->accion) {
    case UI_ACC_NINGUNA:
        return;
    case UI_ACC_MEDIR:       ok = sistema_medir();                 break;
    case UI_ACC_CAL_NEGRO:   ok = sistema_calibrar(true);          break;
    case UI_ACC_CAL_BLANCO:  ok = sistema_calibrar(false);         break;
    case UI_ACC_REFERENCIAR:
        ok = sistema_referenciar();
        if (ok) { mensaje("Referenciando el eje..."); }
        break;
    case UI_ACC_IR_A:
        ok = sistema_ir_a(o->um);
        if (ok) { mensaje("Moviendo el cursor"); }
        break;
    case UI_ACC_GUARDAR:
        if (sistema_guardar()) {
            char buf[41];
            snprintf(buf, sizeof buf, "Lectura guardada (%u)", historial_cantidad(sistema_historial()));
            mensaje(buf);
            buzzer_reproducir(&SONIDO_CLIC);
        } else {
            mensaje("Nada que guardar: mide primero");
            buzzer_reproducir(&SONIDO_ALERTA);
        }
        return;
    case UI_ACC_ARMAR:
    case UI_ACC_DESARMAR: {
        bool armar = o->accion == UI_ACC_ARMAR;
        ok = sistema_armar(armar);
        if (ok) {
            mensaje(armar ? "Motor armado" : "Motor desarmado");
            buzzer_reproducir(armar ? &SONIDO_CONFIRMAR : &SONIDO_CLIC);
        }
        break;
    }
    case UI_ACC_MANUAL:
        ok = sistema_manual(true);
        if (ok) { mensaje("Manual: joystick X · B4 sale"); }
        break;
    }
    if (!ok) {
        rechazada();
    }
}

/* ------------------------------------------------------------------ */
/* Entradas                                                            */
/* ------------------------------------------------------------------ */

static ui_entrada_t leer_entradas(void)
{
    static bool b4_largo_visto;
    static int dir_anterior;
    static uint32_t t_dir;
    ui_entrada_t e = { 0 };

    botones_actualizar();
    e.b1 = boton_pulsado(BOTON_1);
    e.b2 = boton_pulsado(BOTON_2);
    e.b3 = boton_pulsado(BOTON_3);
    e.ok = boton_pulsado(BOTON_JOY);

    /* B4: una pulsación larga NO debe guardar una lectura al soltar. */
    (void)boton_pulsado(BOTON_4);
    if (boton_largo(BOTON_4)) {
        e.b4_largo = true;
        b4_largo_visto = true;
    }
    if (boton_soltado(BOTON_4)) {
        e.b4 = !b4_largo_visto;
        b4_largo_visto = false;
    }

    joystick_t j;
    joystick_leer(&j);
    e.joy_x = j.x;

    /* Un pulso arriba/abajo por movimiento de palanca, con repetición. */
    int dir = j.y > JOY_UMBRAL_DIR ? -1 : (j.y < -JOY_UMBRAL_DIR ? +1 : 0);
    uint32_t ahora = tiempo_us();
    if (dir != 0 && (dir != dir_anterior || (uint32_t)(ahora - t_dir) >= JOY_REPETICION_US)) {
        e.dir = dir;
        t_dir = ahora;
    }
    dir_anterior = dir;
    return e;
}

/* Entradas con significado propio según el estado: cancelar, salir del modo
   manual o reiniciar un error. Consume las pulsaciones que usa. */
static void atender_entradas_de_estado(ui_entrada_t *e)
{
    switch (sistema_estado()) {
    case SIS_HOMING:
    case SIS_MEASURE:
    case SIS_CALIBRATE:
    case SIS_MOVE_TO_TARGET:
        if (e->b4 || e->ok) {
            sistema_cancelar();
            e->b4 = e->ok = false;
        }
        break;

    case SIS_MANUAL:
        sistema_manual_velocidad(e->joy_x * CARRO_VEL_MAX_PRUEBA / 100);
        if (e->b4 || e->ok) {
            sistema_manual(false);
            mensaje("Modo manual terminado");
            e->b4 = e->ok = false;
        }
        break;

    case SIS_ERROR:
        if (e->b4_largo) {
            if (!sistema_reiniciar_error()) {
                mensaje("El error persiste: revisa los finales");
                buzzer_reproducir(&SONIDO_ERROR);
            }
            e->b4_largo = false;
        }
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */

void app_ejecutar(void)
{
    adc_iniciar();
    botones_iniciar();
    joystick_iniciar();         /* mide el centro: joystick quieto al arrancar */
    buzzer_iniciar();
    led_iniciar();
    st7789_iniciar();
    sistema_iniciar();          /* eje, sensor de color, IR de proximidad y historial */
    ui_iniciar();

    modelo.objetivo_um = -1;

    uint32_t t_entrada = 0, t_dibujo = 0;

    while (true) {
        buzzer_actualizar();
        sistema_actualizar();   /* vigila finales, rampa y tiempos: va en cada vuelta */
        atender_eventos();

        /* Las entradas se leen cada 10 ms: el joystick son 8 conversiones del ADC. */
        if ((uint32_t)(tiempo_us() - t_entrada) >= PERIODO_ENTRADA_US) {
            t_entrada = tiempo_us();
            ui_entrada_t entrada = leer_entradas();

            atender_entradas_de_estado(&entrada);
            actualizar_modelo();
            ui_orden_t orden = ui_procesar(&entrada, &modelo);
            ejecutar(&orden);
            actualizar_modelo();
        }

        if ((uint32_t)(tiempo_us() - t_dibujo) >= PERIODO_DIBUJO_US) {
            t_dibujo = tiempo_us();
            ui_dibujar(&modelo);    /* una sección por llamada */
        }

        tiempo_esperar_us(300);
    }
}
