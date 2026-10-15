#include "aplicacion/eje.h"

#include "aplicacion/carro.h"
#include "configuracion/eje.h"
#include "configuracion/motor.h"
#include "dominio/posicion.h"
#include "drivers/finales.h"
#include "hw/tiempo.h"

typedef enum { DEST_POSICION, DEST_INICIO, DEST_FIN } destino_t;

static posicion_t    pos;
static eje_estado_t  estado;
static eje_error_t   error;
static destino_t     tipo_destino;
static int32_t       destino_um;
static bool          via_extremo;        /* primero a un final, después a destino_final_um */
static int32_t       destino_final_um;
static int           manual_vel;
static bool          fin_previo;
static uint32_t      t_ultimo;
static uint32_t      t_cal0;
static uint32_t      duracion_cal_ms;

/* Tiempo máximo de un movimiento normal: lo que tardó la calibración + 40 %. */
static uint32_t timeout_movimiento(void)
{
    if (!pos.calibrada) {
        return CARRO_TIMEOUT_MS;
    }
    return duracion_cal_ms * 7u / 5u + EJE_TIMEOUT_MARGEN_MS;
}

static void fallar(eje_error_t e)
{
    carro_frenar();
    error = e;
    estado = EJE_ERROR;
    carro_timeout_ms(CARRO_TIMEOUT_MS);
}

void eje_iniciar(void)
{
    posicion_iniciar(&pos, EJE_LONGITUD_UM);
    estado = EJE_SIN_REFERENCIA;
    error = EJE_ERROR_NINGUNO;
    manual_vel = 0;
    duracion_cal_ms = 0;
    t_ultimo = tiempo_us();
    carro_timeout_ms(CARRO_TIMEOUT_MS);
}

bool eje_referenciar(void)
{
    if (!carro_armado() || carro_error() != CARRO_ERROR_NINGUNO || estado == EJE_ERROR) {
        return false;
    }
    pos.calibrada = false;
    manual_vel = 0;
    fin_previo = final_fin_activo();
    carro_timeout_ms(EJE_TIMEOUT_HOMING_MS);
    estado = EJE_BUSCANDO_INICIO;
    return true;
}

static void empezar_destino(destino_t tipo, int32_t um)
{
    tipo_destino = tipo;
    destino_um = um;
    carro_timeout_ms(timeout_movimiento());
    estado = EJE_MOVIENDO;
}

bool eje_ir_a(int32_t x_um)
{
    if (!pos.calibrada || estado == EJE_ERROR || !carro_armado() ||
        estado == EJE_BUSCANDO_INICIO || estado == EJE_CALIBRANDO) {
        return false;
    }
    if (x_um < 0) { x_um = 0; }
    if (x_um > pos.longitud_um) { x_um = pos.longitud_um; }

    manual_vel = 0;
    via_extremo = false;
    if (x_um <= EJE_TOLERANCIA_UM) {
        empezar_destino(DEST_INICIO, 0);
    } else if (x_um >= pos.longitud_um - EJE_TOLERANCIA_UM) {
        empezar_destino(DEST_FIN, pos.longitud_um);
    } else {
        empezar_destino(DEST_POSICION, x_um);

#if EJE_REFERENCIA_PREVIA
        /* ¿Compensa pasar antes por el final más cercano al destino? */
        int32_t actual = posicion_um(&pos);
        int32_t directo = actual > x_um ? actual - x_um : x_um - actual;
        bool por_inicio = x_um <= pos.longitud_um - x_um;
        int32_t ultimo_tramo = por_inicio ? x_um : pos.longitud_um - x_um;
        if (ultimo_tramo + EJE_REF_PREVIA_AHORRO_UM < directo) {
            via_extremo = true;
            destino_final_um = x_um;
            tipo_destino = por_inicio ? DEST_INICIO : DEST_FIN;
            destino_um = por_inicio ? 0 : pos.longitud_um;
        }
#endif
    }
    return true;
}

void eje_manual(int velocidad)
{
    manual_vel = velocidad;
}

void eje_parar(void)
{
    manual_vel = 0;
    carro_frenar();
    if (estado == EJE_BUSCANDO_INICIO || estado == EJE_CALIBRANDO) {
        pos.calibrada = false;
        estado = EJE_SIN_REFERENCIA;
        carro_timeout_ms(CARRO_TIMEOUT_MS);
    } else if (estado == EJE_MOVIENDO) {
        estado = EJE_LISTO;
    }
}

bool eje_reiniciar(void)
{
    if (!carro_reiniciar_error()) {
        return false;
    }
    error = EJE_ERROR_NINGUNO;
    estado = EJE_SIN_REFERENCIA;
    pos.calibrada = false;
    manual_vel = 0;
    carro_timeout_ms(CARRO_TIMEOUT_MS);
    return true;
}

/* ------------------------------------------------------------------ */

static void paso_buscando_inicio(void)
{
    bool ini = final_inicio_activo();
    bool fin = final_fin_activo();

    if (ini) {
        /* Inicio alcanzado: cero de la posición y a calibrar hacia el final. */
        carro_frenar();
        posicion_en_inicio(&pos);
        t_cal0 = tiempo_us();
        carro_timeout_ms(EJE_TIMEOUT_CALIB_MS);
        estado = EJE_CALIBRANDO;
        return;
    }
    if (fin && !fin_previo) {
        /* Buscando el inicio apareció el FINAL: el sentido del motor está invertido. */
        fallar(EJE_ERROR_SENTIDO_INVERTIDO);
        return;
    }
    fin_previo = fin;
    carro_mover(-EJE_VEL_HOMING);
}

static void paso_calibrando(uint32_t ahora)
{
    if (final_fin_activo()) {
        uint32_t ms = (ahora - t_cal0) / 1000u;
        carro_frenar();
        if (ms < EJE_RECORRIDO_MIN_MS || !posicion_calibrar(&pos)) {
            fallar(EJE_ERROR_RECORRIDO_CORTO);
            return;
        }
        duracion_cal_ms = ms;
        empezar_destino(DEST_INICIO, 0);    /* vuelve al inicio y queda listo */
        return;
    }
    carro_mover(+EJE_VEL_VIAJE);
}

static int velocidad_hacia(int32_t resto_um)
{
    int32_t dist = resto_um < 0 ? -resto_um : resto_um;
    int v = dist > EJE_DIST_APROX_UM ? EJE_VEL_VIAJE : EJE_VEL_APROX;
    return resto_um > 0 ? v : -v;
}

static void paso_moviendo(void)
{
    int32_t actual = posicion_um(&pos);

    switch (tipo_destino) {
    case DEST_INICIO:
        if (final_inicio_activo()) {
            carro_frenar();
            if (via_extremo) {
                via_extremo = false;
                tipo_destino = DEST_POSICION;
                destino_um = destino_final_um;
            } else {
                estado = EJE_LISTO;
            }
        } else {
            carro_mover(velocidad_hacia(-(actual > 0 ? actual : EJE_DIST_APROX_UM)));
        }
        break;
    case DEST_FIN:
        if (final_fin_activo()) {
            carro_frenar();
            if (via_extremo) {
                via_extremo = false;
                tipo_destino = DEST_POSICION;
                destino_um = destino_final_um;
            } else {
                estado = EJE_LISTO;
            }
        } else {
            carro_mover(velocidad_hacia(pos.longitud_um - actual > 0 ?
                                        pos.longitud_um - actual : EJE_DIST_APROX_UM));
        }
        break;
    case DEST_POSICION: {
        int v = posicion_comando(actual, destino_um, EJE_VEL_VIAJE, EJE_VEL_APROX,
                                 EJE_DIST_APROX_UM, EJE_TOLERANCIA_UM + EJE_DIST_FRENO_UM);
        if (v == 0) {
            carro_frenar();
            estado = EJE_LISTO;
        } else {
            carro_mover(v);
        }
        break;
    }
    }
}

void eje_actualizar(void)
{
    uint32_t ahora = tiempo_us();
    uint32_t dt = ahora - t_ultimo;
    t_ultimo = ahora;

    /* Integra lo que el motor realmente está haciendo y resincroniza en los extremos. */
    posicion_integrar(&pos, carro_velocidad_real(), dt);
    if (final_inicio_activo()) {
        posicion_en_inicio(&pos);
    }

    if (carro_error() != CARRO_ERROR_NINGUNO && estado != EJE_ERROR) {
        error = EJE_ERROR_CARRO;
        estado = EJE_ERROR;
    }

    switch (estado) {
    case EJE_BUSCANDO_INICIO:
        paso_buscando_inicio();
        break;
    case EJE_CALIBRANDO:
        paso_calibrando(ahora);
        break;
    case EJE_MOVIENDO:
        paso_moviendo();
        break;
    case EJE_LISTO:
    case EJE_SIN_REFERENCIA:
        if (manual_vel != 0) {
            carro_mover(manual_vel);
        } else {
            carro_parar();
        }
        break;
    case EJE_ERROR:
        break;
    }

    if (final_fin_activo()) {
        posicion_en_fin(&pos);
    }

    carro_actualizar();
}

/* ------------------------------------------------------------------ */

eje_estado_t eje_estado(void) { return estado; }
eje_error_t  eje_error(void)  { return error;  }
bool    eje_calibrado(void)   { return pos.calibrada; }
int32_t eje_posicion_um(void) { return posicion_um(&pos); }
bool    eje_en_destino(void)  { return estado == EJE_LISTO; }

bool eje_quieto(void)
{
    bool sin_orden = estado == EJE_LISTO || estado == EJE_SIN_REFERENCIA || estado == EJE_ERROR;
    return sin_orden && manual_vel == 0 && carro_velocidad_real() == 0;
}
uint32_t eje_recorrido_ms(void) { return duracion_cal_ms; }

const char *eje_estado_texto(eje_estado_t e)
{
    switch (e) {
    case EJE_SIN_REFERENCIA:  return "SIN_REFERENCIA";
    case EJE_BUSCANDO_INICIO: return "BUSCANDO_INICIO";
    case EJE_CALIBRANDO:      return "CALIBRANDO";
    case EJE_MOVIENDO:        return "MOVIENDO";
    case EJE_LISTO:           return "LISTO";
    case EJE_ERROR:           return "ERROR";
    }
    return "?";
}

const char *eje_error_texto(eje_error_t e)
{
    switch (e) {
    case EJE_ERROR_NINGUNO:           return "ninguno";
    case EJE_ERROR_CARRO:             return carro_error_texto(carro_error());
    case EJE_ERROR_SENTIDO_INVERTIDO: return "sentido del motor invertido (MOTOR_SIGNO_HACIA_FIN)";
    case EJE_ERROR_RECORRIDO_CORTO:   return "calibracion demasiado corta (revisar finales de carrera)";
    }
    return "?";
}
