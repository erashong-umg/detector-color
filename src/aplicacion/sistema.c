#include "aplicacion/sistema.h"

#include "aplicacion/carro.h"
#include "aplicacion/eje.h"
#include "configuracion/proximidad.h"
#include "configuracion/sistema.h"
#include "dominio/limites.h"
#include "drivers/finales.h"
#include "drivers/proximidad.h"
#include "hw/tiempo.h"

typedef enum { ACC_NINGUNA, ACC_MEDIR, ACC_CAL_NEGRO, ACC_CAL_BLANCO } accion_t;

#define COLA_EVENTOS 8

static sistema_estado_t  estado;
static accion_t          accion;
static sistema_guia_t    guia;
static sistema_guia_t    guia_emitida;
static sistema_rechazo_t rechazo;
static det_resultado_t   apuntado;

static lectura_t   lectura;
static bool        hay_lectura;
static int32_t     objetivo_um = -1;
static historial_t historial;

static uint32_t t_espera0;
static uint32_t t_ok0;
static uint32_t t_coherente0;       /* desde cuándo los finales vuelven a ser coherentes */
static bool     coherente_visto;
static bool     ok_activo;
static bool     auto_referenciando;  /* hay una referencia automática en curso */
static uint8_t  auto_intentos;       /* reintentos hechos */
static uint32_t auto_espera_ms;      /* espera antes del próximo intento */
static bool     auto_pendiente;  /* falta la referencia automática del arranque */
static uint32_t t_auto;
static bool     analisis_avisado;   /* ya se emitió ANALIZANDO: la próxima vuelta mide */
static bool     forzar_objeto;   /* B1 largo: medir sin esperar al IR (muestras oscuras) */
static int      manual_vel;

static sistema_evento_t cola[COLA_EVENTOS];
static unsigned         cola_inicio, cola_cantidad;

/* ------------------------------------------------------------------ */
/* Eventos                                                             */
/* ------------------------------------------------------------------ */

static void emitir(sistema_evento_t e)
{
    if (cola_cantidad == COLA_EVENTOS) {            /* llena: se pierde el más antiguo */
        cola_inicio = (cola_inicio + 1) % COLA_EVENTOS;
        cola_cantidad--;
    }
    cola[(cola_inicio + cola_cantidad) % COLA_EVENTOS] = e;
    cola_cantidad++;
}

sistema_evento_t sistema_tomar_evento(void)
{
    if (cola_cantidad == 0) {
        return SIS_EVT_NINGUNO;
    }
    sistema_evento_t e = cola[cola_inicio];
    cola_inicio = (cola_inicio + 1) % COLA_EVENTOS;
    cola_cantidad--;
    return e;
}

static bool rechazar(sistema_rechazo_t motivo)
{
    rechazo = motivo;
    return false;
}

/* ------------------------------------------------------------------ */
/* Transiciones                                                        */
/* ------------------------------------------------------------------ */

static void volver_a_idle(void)
{
    accion = ACC_NINGUNA;
    guia = guia_emitida = SIS_GUIA_NINGUNA;
    ok_activo = false;
    analisis_avisado = false;
    manual_vel = 0;
    estado = SIS_IDLE;
}

static void entrar_error(void)
{
    auto_referenciando = false;
    auto_pendiente = false;
    accion = ACC_NINGUNA;
    guia = guia_emitida = SIS_GUIA_NINGUNA;
    ok_activo = false;
    manual_vel = 0;
    eje_manual(0);
    estado = SIS_ERROR;
    emitir(SIS_EVT_ERROR);
}

static void cancelar_operacion(bool avisar)
{
    auto_referenciando = false;     /* si el usuario cancela, no se reintenta sola */
    auto_pendiente = false;
    switch (estado) {
    case SIS_HOMING:
    case SIS_MOVE_TO_TARGET:
        eje_parar();
        break;
    case SIS_MANUAL:
        eje_manual(0);
        break;
    case SIS_MEASURE:
    case SIS_CALIBRATE:
    default:
        break;
    }
    if (estado != SIS_IDLE && estado != SIS_ERROR && estado != SIS_INIT) {
        volver_a_idle();
        if (avisar) {
            emitir(SIS_EVT_CANCELADO);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Medir y calibrar: esperar al objeto y actuar                        */
/* ------------------------------------------------------------------ */

static void ejecutar_accion(void)
{
    accion_t a = accion;
    accion = ACC_NINGUNA;
    guia = guia_emitida = SIS_GUIA_NINGUNA;
    ok_activo = false;

    if (a == ACC_MEDIR) {
        if (!deteccion_medir(&lectura)) {
            volver_a_idle();
            emitir(SIS_EVT_LECTURA_INVALIDA);
            return;
        }
        hay_lectura = true;
        objetivo_um = lectura.escala.x_um;
        apuntado = deteccion_apuntar(&lectura);

        if (apuntado == DET_MOVIENDO) {
            estado = SIS_MOVE_TO_TARGET;
            emitir(SIS_EVT_MEDIDA);
        } else {
            volver_a_idle();
            emitir(SIS_EVT_MEDIDA);
        }
        return;
    }

    bool negro = (a == ACC_CAL_NEGRO);
    bool ok = negro ? deteccion_calibrar_negro() : deteccion_calibrar_blanco();
    volver_a_idle();
    emitir(ok ? (negro ? SIS_EVT_CAL_NEGRO : SIS_EVT_CAL_BLANCO) : SIS_EVT_LECTURA_INVALIDA);
}

static void paso_espera_objeto(uint32_t ahora)
{
    bool listo = true;

    if (PROX_HABILITADO && !forzar_objeto) {
        prox_estado_t p = proximidad_estado();

        switch (p) {
        case PROX_SIN_OBJETO: guia = SIS_GUIA_COLOCAR_OBJETO; break;
        case PROX_LEJOS:      guia = SIS_GUIA_ACERCAR;        break;
        case PROX_CERCA:      guia = SIS_GUIA_ALEJAR;         break;
        case PROX_OK:         guia = SIS_GUIA_NINGUNA;        break;
        }
        if (guia != guia_emitida) {
            guia_emitida = guia;
            emitir(SIS_EVT_GUIA);
        }

        if (p == PROX_OK) {
            if (!ok_activo) {
                ok_activo = true;
                t_ok0 = ahora;
            }
            listo = (uint32_t)(ahora - t_ok0) >= SISTEMA_PROX_ESTABLE_MS * 1000u;
        } else {
            ok_activo = false;
            listo = false;
        }
    }

    if (!eje_quieto()) {
        listo = false;
    }

    if (listo && !analisis_avisado) {
        /* Medir bloquea ≈ 270 ms: se avisa una vuelta antes para que el sonido ya esté
           sonando mientras se analiza. */
        analisis_avisado = true;
        emitir(SIS_EVT_ANALIZANDO);
    } else if (listo) {
        analisis_avisado = false;
        ejecutar_accion();
    } else {
        analisis_avisado = false;
        if ((uint32_t)(ahora - t_espera0) >= SISTEMA_ESPERA_OBJETO_MS * 1000u) {
            volver_a_idle();
            emitir(SIS_EVT_TIEMPO_AGOTADO);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Ciclo principal                                                     */
/* ------------------------------------------------------------------ */

void sistema_iniciar(void)
{
    carro_iniciar();
    eje_iniciar();
    deteccion_iniciar();
    proximidad_iniciar();
    historial_iniciar(&historial);

    estado = SIS_INIT;
    accion = ACC_NINGUNA;
    guia = guia_emitida = SIS_GUIA_NINGUNA;
    rechazo = SIS_RECHAZO_NINGUNO;
    apuntado = DET_NO_VALIDA;
    hay_lectura = false;
    objetivo_um = -1;
    manual_vel = 0;
    ok_activo = false;
    cola_inicio = cola_cantidad = 0;
    auto_pendiente = SISTEMA_AUTOREFERENCIAR != 0;
    auto_referenciando = false;
    auto_intentos = 0;
    auto_espera_ms = SISTEMA_AUTOREF_RETARDO_MS;
}

void sistema_actualizar(void)
{
    uint32_t ahora = tiempo_us();

    proximidad_actualizar();
    eje_actualizar();           /* vigila finales, rampa y tiempos: en cada vuelta */

    bool hay_error = carro_error() != CARRO_ERROR_NINGUNO || eje_estado() == EJE_ERROR;

    switch (estado) {
    case SIS_INIT:
        if (hay_error) {
            entrar_error();
        } else {
            estado = SIS_IDLE;
            t_auto = ahora;
            emitir(SIS_EVT_LISTO);
        }
        return;

    case SIS_ERROR:
        /* Recuperación automática: un final desconectado se lee como activo; al reconectarlo y
           pasar SISTEMA_RECUPERAR_MS con los finales coherentes, se reinicia el error y se
           vuelve a referenciar. Solo para este fallo: los demás necesitan a una persona. */
        if (SISTEMA_AUTORECUPERAR && carro_error() == CARRO_ERROR_FINALES_INCOHERENTES) {
            if (limites_incoherentes(final_inicio_activo(), final_fin_activo())) {
                coherente_visto = false;
            } else if (!coherente_visto) {
                coherente_visto = true;
                t_coherente0 = ahora;
            } else if ((uint32_t)(ahora - t_coherente0) >= SISTEMA_RECUPERAR_MS * 1000u && eje_reiniciar()) {
                coherente_visto = false;
                volver_a_idle();
                auto_pendiente = true;
                auto_intentos = 0;
                auto_espera_ms = SISTEMA_AUTOREF_REINTENTO_MS;
                t_auto = ahora;
                emitir(SIS_EVT_RECUPERADO);
            }
        } else {
            coherente_visto = false;
        }
        return;

    default:
        break;
    }

    /* Fallo recuperable durante la referencia automática (tiempo agotado o calibración
       demasiado corta): se reinicia el error y se reintenta, sin molestar a nadie. */
    if (hay_error && auto_referenciando && estado == SIS_HOMING &&
        auto_intentos < SISTEMA_AUTOREF_INTENTOS &&
        (carro_error() == CARRO_ERROR_TIMEOUT || eje_error() == EJE_ERROR_RECORRIDO_CORTO)) {
        auto_intentos++;
        if (eje_reiniciar()) {
            volver_a_idle();
            auto_referenciando = false;
            auto_pendiente = true;
            auto_espera_ms = SISTEMA_AUTOREF_REINTENTO_MS;
            t_auto = ahora;
            emitir(SIS_EVT_AUTOREF_REINTENTO);
            return;
        }
    }

    if (hay_error) {
        entrar_error();
        return;
    }

    switch (estado) {
    case SIS_HOMING:
        if (eje_estado() == EJE_LISTO && eje_calibrado()) {
            volver_a_idle();
            auto_referenciando = false;
            emitir(SIS_EVT_REFERENCIADO);
        }
        break;

    case SIS_MEASURE:
    case SIS_CALIBRATE:
        paso_espera_objeto(ahora);
        break;

    case SIS_MOVE_TO_TARGET:
        if (eje_estado() == EJE_LISTO) {
            volver_a_idle();
            emitir(SIS_EVT_LLEGADA);
        }
        break;

    case SIS_MANUAL:
        eje_manual(manual_vel);
        break;

    case SIS_IDLE:
        /* Referencia automática del arranque: se intenta una sola vez. */
        if (auto_pendiente && (uint32_t)(ahora - t_auto) >= auto_espera_ms * 1000u) {
            auto_pendiente = false;
            if (sistema_armar(true)) {
                if (sistema_referenciar()) {
                    auto_referenciando = true;
                    emitir(SIS_EVT_AUTOREFERENCIANDO);
                } else {
                    sistema_armar(false);
                }
            }
        }
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Órdenes                                                             */
/* ------------------------------------------------------------------ */

static bool exigir_idle(void)
{
    if (estado == SIS_ERROR) {
        return rechazar(SIS_RECHAZO_ERROR_ACTIVO);
    }
    if (estado != SIS_IDLE) {
        return rechazar(SIS_RECHAZO_OCUPADO);
    }
    return true;
}

bool sistema_armar(bool armar)
{
    if (armar && (estado == SIS_ERROR || carro_error() != CARRO_ERROR_NINGUNO)) {
        return rechazar(SIS_RECHAZO_ERROR_ACTIVO);
    }
    if (!carro_armar(armar)) {
        return rechazar(SIS_RECHAZO_ERROR_ACTIVO);
    }
    if (!armar) {
        cancelar_operacion(false);
    }
    return true;
}

bool sistema_referenciar(void)
{
    if (!exigir_idle()) {
        return false;
    }
    if (!carro_armado()) {
        return rechazar(SIS_RECHAZO_NO_ARMADO);
    }
    if (!eje_referenciar()) {
        return rechazar(SIS_RECHAZO_ERROR_ACTIVO);
    }
    estado = SIS_HOMING;
    return true;
}

static bool empezar_espera(accion_t a)
{
    accion = a;
    estado = (a == ACC_MEDIR) ? SIS_MEASURE : SIS_CALIBRATE;
    t_espera0 = tiempo_us();
    ok_activo = false;
    analisis_avisado = false;
    forzar_objeto = false;
    guia = guia_emitida = SIS_GUIA_NINGUNA;
    return true;
}

void sistema_forzar_objeto(void)
{
    if (estado == SIS_MEASURE || estado == SIS_CALIBRATE) {
        forzar_objeto = true;
        guia = SIS_GUIA_NINGUNA;
    }
}

bool sistema_medir(void)
{
    if (!exigir_idle()) {
        return false;
    }
    if (!deteccion_calibrado()) {
        return rechazar(SIS_RECHAZO_NO_CALIBRADO);
    }
    return empezar_espera(ACC_MEDIR);
}

bool sistema_calibrar(bool negro)
{
    if (!exigir_idle()) {
        return false;
    }
    return empezar_espera(negro ? ACC_CAL_NEGRO : ACC_CAL_BLANCO);
}

bool sistema_ir_a(int32_t x_um)
{
    if (estado == SIS_ERROR) {
        return rechazar(SIS_RECHAZO_ERROR_ACTIVO);
    }
    if (estado != SIS_IDLE && estado != SIS_MOVE_TO_TARGET) {
        return rechazar(SIS_RECHAZO_OCUPADO);
    }
    if (!carro_armado()) {
        return rechazar(SIS_RECHAZO_NO_ARMADO);
    }
    if (!eje_calibrado()) {
        return rechazar(SIS_RECHAZO_SIN_REFERENCIA);
    }
    if (!eje_ir_a(x_um)) {
        return rechazar(SIS_RECHAZO_OCUPADO);
    }
    objetivo_um = x_um;
    estado = SIS_MOVE_TO_TARGET;
    return true;
}

bool sistema_manual(bool activar)
{
    if (!activar) {
        if (estado == SIS_MANUAL) {
            eje_manual(0);
            volver_a_idle();
        }
        return true;
    }
    if (!exigir_idle()) {
        return false;
    }
    if (!carro_armado()) {
        return rechazar(SIS_RECHAZO_NO_ARMADO);
    }
    manual_vel = 0;
    estado = SIS_MANUAL;
    return true;
}

bool sistema_manual_ir_a(int32_t x_um)
{
    if (estado != SIS_MANUAL) {
        return rechazar(SIS_RECHAZO_OCUPADO);
    }
    if (!eje_calibrado()) {
        return rechazar(SIS_RECHAZO_SIN_REFERENCIA);
    }
    if (!eje_ir_a(x_um)) {
        return rechazar(SIS_RECHAZO_ERROR_ACTIVO);
    }
    return true;
}

void sistema_manual_velocidad(int velocidad)
{
    if (estado != SIS_MANUAL) {
        return;
    }
    if (velocidad > 100)  { velocidad = 100; }
    if (velocidad < -100) { velocidad = -100; }
    manual_vel = velocidad;
}

bool sistema_guardar(void)
{
    if (!hay_lectura) {
        return false;
    }
    entrada_hist_t e = {
        .rgb  = lectura.rgb,
        .h    = lectura.hsv.h,
        .x_um = lectura.escala.x_um,
    };
    historial_agregar(&historial, &e);
    return true;
}

void sistema_cancelar(void)
{
    cancelar_operacion(true);
}

bool sistema_reiniciar_error(void)
{
    if (estado != SIS_ERROR) {
        return true;
    }
    if (!eje_reiniciar()) {
        return rechazar(SIS_RECHAZO_ERROR_ACTIVO);
    }
    volver_a_idle();
    emitir(SIS_EVT_ERROR_REINICIADO);
    return true;
}

/* ------------------------------------------------------------------ */
/* Consultas                                                           */
/* ------------------------------------------------------------------ */

sistema_estado_t  sistema_estado(void)    { return estado; }
sistema_guia_t    sistema_guia(void)      { return guia; }
sistema_rechazo_t sistema_rechazo(void)   { return rechazo; }
det_resultado_t   sistema_apuntado(void)  { return apuntado; }
uint8_t           sistema_autoref_intentos(void) { return auto_intentos; }
bool              sistema_hay_lectura(void) { return hay_lectura; }
const lectura_t  *sistema_lectura(void)   { return &lectura; }
int32_t           sistema_objetivo_um(void) { return objetivo_um; }
const historial_t *sistema_historial(void) { return &historial; }
bool              sistema_armado(void)    { return carro_armado(); }
bool              sistema_referenciado(void) { return eje_calibrado(); }

const char *sistema_estado_texto(sistema_estado_t e)
{
    switch (e) {
    case SIS_INIT:           return "INIT";
    case SIS_HOMING:         return "HOMING";
    case SIS_IDLE:           return "IDLE";
    case SIS_MEASURE:        return "MEASURE";
    case SIS_CALIBRATE:      return "CALIBRATE";
    case SIS_MOVE_TO_TARGET: return "MOVE_TO_TARGET";
    case SIS_MANUAL:         return "MANUAL";
    case SIS_ERROR:          return "ERROR";
    }
    return "?";
}
