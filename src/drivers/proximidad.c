#include "drivers/proximidad.h"

#include "configuracion/pines.h"
#include "configuracion/proximidad.h"
#include "drivers/adc.h"
#include "hw/pin.h"
#include "hw/tiempo.h"

#define PROX_CANAL_ADC  (PIN_IR_PROX_A - RP_ADC_PRIMER_PIN)

_Static_assert(PIN_IR_PROX_A >= RP_ADC_PRIMER_PIN && PIN_IR_PROX_A <= RP_ADC_PRIMER_PIN + 2,
               "proximidad.c: la salida analógica debe estar en GP26..GP28");

static prox_filtro_t filtro;
static uint16_t adc_crudo;
static uint16_t nivel;
static bool     digital_activo;
static uint32_t t_ultimo;

void proximidad_iniciar(void)
{
    pin_analogico(PIN_IR_PROX_A);
    /* Pull hacia "sin objeto": con el cable suelto se lee como que no hay nada. */
    pin_entrada(PIN_IR_PROX_D, PROX_DIGITAL_ACTIVO_BAJO ? PIN_PULL_UP : PIN_PULL_DOWN);
    prox_filtro_iniciar(&filtro);
    t_ultimo = tiempo_us() - PROX_PERIODO_MS * 1000u;
    adc_crudo = 0;
    nivel = 0;
    digital_activo = false;
}

void proximidad_actualizar(void)
{
    uint32_t ahora = tiempo_us();
    if ((uint32_t)(ahora - t_ultimo) < PROX_PERIODO_MS * 1000u) {
        return;
    }
    t_ultimo = ahora;

    adc_crudo = adc_leer_promedio(PROX_CANAL_ADC, PROX_MUESTRAS_ADC);
    nivel = PROX_ANALOGICO_INVERTIDO ? (uint16_t)(4095u - adc_crudo) : adc_crudo;
    bool od = pin_leer(PIN_IR_PROX_D);
    digital_activo = PROX_DIGITAL_ACTIVO_BAJO ? !od : od;

    prox_filtro_actualizar(&filtro, nivel, digital_activo);
}

prox_estado_t proximidad_estado(void)
{
    return PROX_HABILITADO ? filtro.estado : PROX_OK;
}

uint16_t proximidad_adc_crudo(void)       { return adc_crudo; }
bool     proximidad_od_nivel(void)        { return pin_leer(PIN_IR_PROX_D); }
uint16_t proximidad_nivel(void)           { return nivel; }
bool     proximidad_digital_activo(void)  { return digital_activo; }
