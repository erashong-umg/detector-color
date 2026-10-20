#include "aplicacion/protocolo.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "aplicacion/carro.h"
#include "aplicacion/deteccion.h"
#include "aplicacion/eje.h"
#include "configuracion/eje.h"
#include "configuracion/sistema.h"
#include "dominio/color.h"
#include "dominio/escala.h"
#include "dominio/historial.h"
#include "dominio/proximidad.h"
#include "drivers/finales.h"
#include "drivers/proximidad.h"

#define LINEA_MAX    64         /* longitud máxima de un comando recibido      */
#define SALIDA_MAX   200        /* longitud máxima de una línea enviada         */

static protocolo_salida_t salida;
static char   entrada[LINEA_MAX];
static size_t entrada_n;
static bool   entrada_desbordada;

/* ------------------------------------------------------------------ */
/* Salida                                                              */
/* ------------------------------------------------------------------ */

static void emitir(const char *formato, ...) __attribute__((format(printf, 1, 2)));

static void emitir(const char *formato, ...)
{
    char linea[SALIDA_MAX];
    va_list args;

    if (!salida) {
        return;
    }
    va_start(args, formato);
    vsnprintf(linea, sizeof linea, formato, args);
    va_end(args);
    salida(linea);
}

/* Milímetros con un decimal; "-" si no hay valor (um < 0). */
static void fmt_mm(char *buf, size_t n, int32_t um)
{
    if (um < 0) {
        snprintf(buf, n, "-");
        return;
    }
    int32_t decimas = (um + 50) / 100;
    snprintf(buf, n, "%ld.%ld", (long)(decimas / 10), (long)(decimas % 10));
}

static const char *temperatura_texto(temperatura_t t)
{
    switch (t) {
    case TEMP_CALIDO: return "CALIDO";
    case TEMP_NEUTRO: return "NEUTRO";
    case TEMP_FRIO:   return "FRIO";
    }
    return "?";
}

static const char *clase_texto(escala_clase_t c)
{
    switch (c) {
    case ESCALA_CROMATICO: return "CROMATICO";
    case ESCALA_NEGRO:     return "NEGRO";
    case ESCALA_BLANCO:    return "BLANCO";
    case ESCALA_GRIS:      return "GRIS";
    }
    return "?";
}

static const char *motivo_rechazo(sistema_rechazo_t r)
{
    switch (r) {
    case SIS_RECHAZO_NINGUNO:        return "RECHAZADO";
    case SIS_RECHAZO_OCUPADO:        return "OCUPADO";
    case SIS_RECHAZO_ERROR_ACTIVO:   return "ERROR_ACTIVO";
    case SIS_RECHAZO_NO_ARMADO:      return "NO_ARMADO";
    case SIS_RECHAZO_SIN_REFERENCIA: return "SIN_REFERENCIA";
    case SIS_RECHAZO_NO_CALIBRADO:   return "NO_CALIBRADO";
    }
    return "RECHAZADO";
}

static void responder(bool ok)
{
    if (ok) {
        emitir("OK");
    } else {
        emitir("ERR,%s", motivo_rechazo(sistema_rechazo()));
    }
}

static const char *error_texto(void)
{
    if (carro_error() != CARRO_ERROR_NINGUNO) {
        return carro_error_texto(carro_error());
    }
    return eje_error_texto(eje_error());
}

/* ------------------------------------------------------------------ */
/* Datos de una lectura                                                */
/* ------------------------------------------------------------------ */

static void cuerpo_lectura(char *buf, size_t n)
{
    const lectura_t *l = sistema_lectura();
    char obj[16];

    fmt_mm(obj, sizeof obj, sistema_objetivo_um());
    snprintf(buf, n, "hex=%s,rgb=%u,%u,%u,hsv=%d,%u,%u,hz=%lu,%lu,%lu,%lu,clase=%s,temp=%s,obj_mm=%s",
             l->hex, l->rgb.r, l->rgb.g, l->rgb.b, l->hsv.h, l->hsv.s, l->hsv.v,
             (unsigned long)l->f.r, (unsigned long)l->f.g, (unsigned long)l->f.b,
             (unsigned long)l->f.c, clase_texto(l->escala.clase),
             temperatura_texto(l->escala.temperatura), obj);
}

static void enviar_estado(void)
{
    char pos[16], obj[16];

    fmt_mm(pos, sizeof pos, eje_posicion_um());
    fmt_mm(obj, sizeof obj, sistema_objetivo_um());
    emitir("ESTADO,estado=%s,armado=%d,referenciado=%d,calibrado=%d,pos_mm=%s,obj_mm=%s,eje=%s,ir=%c%c,prox=%s,cal_ms=%lu",
           sistema_estado_texto(sistema_estado()), sistema_armado() ? 1 : 0,
           sistema_referenciado() ? 1 : 0, deteccion_calibrado() ? 1 : 0, pos, obj,
           eje_estado_texto(eje_estado()),
           final_inicio_activo() ? 'I' : '-', final_fin_activo() ? 'F' : '-',
           prox_texto(proximidad_estado()), (unsigned long)eje_ultima_calibracion_ms());
}

static void enviar_lectura(void)
{
    char cuerpo[SALIDA_MAX];

    if (!sistema_hay_lectura()) {
        emitir("ERR,SIN_LECTURA");
        return;
    }
    cuerpo_lectura(cuerpo, sizeof cuerpo);
    emitir("LECTURA,%s", cuerpo);
}

static void enviar_historial(void)
{
    const historial_t *h = sistema_historial();
    uint8_t n = historial_cantidad(h);

    for (uint8_t i = 0; i < n; i++) {
        entrada_hist_t e;
        char hex[8], pos[16];

        if (!historial_obtener(h, i, &e)) {
            continue;
        }
        color_a_hex(e.rgb, hex);
        fmt_mm(pos, sizeof pos, e.x_um);
        emitir("HIST,%u,hex=%s,h=%d,pos_mm=%s", (unsigned)i, hex, e.h, pos);
    }
    emitir("HIST,FIN,%u", (unsigned)n);
}

/* ------------------------------------------------------------------ */
/* Comandos                                                            */
/* ------------------------------------------------------------------ */

/* "160", "160.5" o "0.250" → µm. Solo dígitos y un punto, hasta 3 decimales. */
static bool leer_mm(const char *s, int32_t *um)
{
    int32_t entero = 0, frac = 0, escala = 1000;
    bool hay_digitos = false;

    for (; isdigit((unsigned char)*s); s++) {
        entero = entero * 10 + (*s - '0');
        hay_digitos = true;
        if (entero > 9999) {
            return false;
        }
    }
    if (*s == '.') {
        for (s++; isdigit((unsigned char)*s); s++) {
            if (escala > 1) {
                escala /= 10;
                frac += (*s - '0') * escala;
            }
            hay_digitos = true;
        }
    }
    if (!hay_digitos || *s != '\0') {
        return false;
    }
    *um = entero * 1000 + frac;
    return true;
}

static void enviar_ayuda(void)
{
    emitir("AYUDA,ESTADO LECTURA HIST MEDIR FORZAR PARAR GUARDAR REF ARMAR DESARMAR REINICIAR");
    emitir("AYUDA,CAL NEGRO|BLANCO  IR <mm>");
}

void protocolo_procesar_linea(const char *linea)
{
    char texto[LINEA_MAX];
    char *cmd, *arg, *fin;
    size_t i;

    /* Mayúsculas y sin espacios en los extremos. */
    for (i = 0; linea[i] != '\0' && i < sizeof texto - 1; i++) {
        texto[i] = (char)toupper((unsigned char)linea[i]);
    }
    texto[i] = '\0';
    cmd = texto;
    while (*cmd == ' ' || *cmd == '\t') {
        cmd++;
    }
    for (fin = cmd + strlen(cmd); fin > cmd && (fin[-1] == ' ' || fin[-1] == '\t'); fin--) {
        fin[-1] = '\0';
    }
    if (*cmd == '\0') {
        return;
    }
    arg = cmd;
    while (*arg != '\0' && *arg != ' ' && *arg != '\t') {
        arg++;
    }
    if (*arg != '\0') {
        *arg++ = '\0';
        while (*arg == ' ' || *arg == '\t') {
            arg++;
        }
    }

    if (strcmp(cmd, "AYUDA") == 0 || strcmp(cmd, "?") == 0) {
        enviar_ayuda();
    } else if (strcmp(cmd, "ESTADO") == 0) {
        enviar_estado();
    } else if (strcmp(cmd, "LECTURA") == 0) {
        enviar_lectura();
    } else if (strcmp(cmd, "HIST") == 0) {
        enviar_historial();
    } else if (strcmp(cmd, "MEDIR") == 0) {
        responder(sistema_medir());
    } else if (strcmp(cmd, "CAL") == 0) {
        if (strcmp(arg, "NEGRO") == 0) {
            responder(sistema_calibrar(true));
        } else if (strcmp(arg, "BLANCO") == 0) {
            responder(sistema_calibrar(false));
        } else {
            emitir("ERR,ARGUMENTO");
        }
    } else if (strcmp(cmd, "FORZAR") == 0) {
        sistema_forzar_objeto();
        emitir("OK");
    } else if (strcmp(cmd, "ARMAR") == 0) {
        responder(sistema_armar(true));
    } else if (strcmp(cmd, "DESARMAR") == 0) {
        responder(sistema_armar(false));
    } else if (strcmp(cmd, "REF") == 0) {
        responder(sistema_referenciar());
    } else if (strcmp(cmd, "IR") == 0) {
        int32_t um;
        if (!leer_mm(arg, &um)) {
            emitir("ERR,ARGUMENTO");
        } else if (um > EJE_LONGITUD_UM) {
            emitir("ERR,FUERA_DE_RANGO");
        } else {
            responder(sistema_ir_a(um));
        }
    } else if (strcmp(cmd, "PARAR") == 0) {
        sistema_cancelar();
        emitir("OK");
    } else if (strcmp(cmd, "GUARDAR") == 0) {
        if (sistema_guardar()) {
            emitir("OK");
        } else {
            emitir("ERR,SIN_LECTURA");
        }
    } else if (strcmp(cmd, "REINICIAR") == 0) {
        responder(sistema_reiniciar_error());
    } else {
        emitir("ERR,COMANDO");
    }
}

/* ------------------------------------------------------------------ */
/* Entrada byte a byte                                                 */
/* ------------------------------------------------------------------ */

void protocolo_iniciar(protocolo_salida_t funcion_salida)
{
    salida = funcion_salida;
    entrada_n = 0;
    entrada_desbordada = false;
    emitir("HOLA,detector-color,protocolo=1");
}

void protocolo_byte(int c)
{
    if (c == '\n' || c == '\r') {
        if (entrada_desbordada) {
            emitir("ERR,LINEA_LARGA");
        } else if (entrada_n > 0) {
            entrada[entrada_n] = '\0';
            protocolo_procesar_linea(entrada);
        }
        entrada_n = 0;
        entrada_desbordada = false;
        return;
    }
    if (c < 0 || c > 255) {
        return;
    }
    if (entrada_n < sizeof entrada - 1) {
        entrada[entrada_n++] = (char)c;
    } else {
        entrada_desbordada = true;
    }
}

/* ------------------------------------------------------------------ */
/* Eventos                                                             */
/* ------------------------------------------------------------------ */

void protocolo_evento(sistema_evento_t e)
{
    char cuerpo[SALIDA_MAX];

    switch (e) {
    case SIS_EVT_NINGUNO:
        break;
    case SIS_EVT_LISTO:             emitir("EVT,LISTO");             break;
    case SIS_EVT_AUTOREFERENCIANDO:
        /* Desde dónde arranca, según los finales: INICIO, FIN o MEDIO (a medio recorrido). */
        emitir("EVT,AUTOREFERENCIANDO,desde=%s",
               final_inicio_activo() ? "INICIO" : (final_fin_activo() ? "FIN" : "MEDIO"));
        break;
    case SIS_EVT_AUTOREF_REINTENTO:
        emitir("EVT,AUTOREF_REINTENTO,n=%u,max=%u", (unsigned)sistema_autoref_intentos(),
               (unsigned)SISTEMA_AUTOREF_INTENTOS);
        break;
    case SIS_EVT_REFERENCIADO:      emitir("EVT,REFERENCIADO");      break;
    case SIS_EVT_ANALIZANDO:        emitir("EVT,ANALIZANDO");        break;
    case SIS_EVT_MEDIDA:
        if (sistema_hay_lectura()) {
            cuerpo_lectura(cuerpo, sizeof cuerpo);
            emitir("EVT,MEDIDA,%s", cuerpo);
        } else {
            emitir("EVT,MEDIDA");
        }
        break;
    case SIS_EVT_CAL_NEGRO:         emitir("EVT,CAL_NEGRO");         break;
    case SIS_EVT_CAL_BLANCO:        emitir("EVT,CAL_BLANCO");        break;
    case SIS_EVT_LLEGADA:           emitir("EVT,LLEGADA");           break;
    case SIS_EVT_GUIA:
        switch (sistema_guia()) {
        case SIS_GUIA_NINGUNA:        emitir("EVT,GUIA,NINGUNA");  break;
        case SIS_GUIA_COLOCAR_OBJETO: emitir("EVT,GUIA,COLOCAR");  break;
        case SIS_GUIA_ACERCAR:        emitir("EVT,GUIA,ACERCAR");  break;
        case SIS_GUIA_ALEJAR:         emitir("EVT,GUIA,ALEJAR");   break;
        }
        break;
    case SIS_EVT_TIEMPO_AGOTADO:    emitir("EVT,TIEMPO_AGOTADO");    break;
    case SIS_EVT_CANCELADO:         emitir("EVT,CANCELADO");         break;
    case SIS_EVT_LECTURA_INVALIDA:  emitir("EVT,LECTURA_INVALIDA");  break;
    case SIS_EVT_ERROR:             emitir("EVT,ERROR,%s", error_texto()); break;
    case SIS_EVT_RECUPERADO:        emitir("EVT,RECUPERADO");        break;
    case SIS_EVT_ERROR_REINICIADO:  emitir("EVT,ERROR_REINICIADO");  break;
    }
}
