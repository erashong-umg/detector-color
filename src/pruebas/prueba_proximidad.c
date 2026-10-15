/*
 * prueba_proximidad.c — Diagnóstico y ajuste del IR de proximidad junto al TCS3200.
 *
 * Mide la polaridad y los umbrales de TU módulo y te dice qué poner en
 * configuracion/proximidad.h. Abrir el puerto serie (115200).
 *
 * Procedimiento, con la muestra que vayas a usar (papel blanco, por ejemplo):
 *   B2  retirar todo objeto de delante del sensor y pulsar → registra "sin objeto"
 *   B3  colocar el objeto a la DISTANCIA CORRECTA para medir el color y pulsar
 *   B4  acercarlo demasiado (casi tocando) y pulsar → registra "demasiado cerca"
 *   Con las tres tomas, imprime los valores recomendados. B1 reinicia las tomas.
 *
 * Mientras tanto imprime cada 250 ms: ADC crudo, nivel orientado, nivel eléctrico
 * de OD y el estado que clasifica el firmware con la configuración actual. El LED
 * muestra el estado: apagado = nada, ámbar = lejos, verde = ok, rojo = cerca.
 *
 * IMPORTANTE: la lectura depende del color de la muestra (el negro refleja poco
 * IR y parece más lejos). Repetir con una muestra clara y una oscura y elegir
 * umbrales que sirvan para las dos, o medir siempre con muestras parecidas.
 * Comprobar también que el IR no altera la medida del TCS3200 (docs/hardware.md §4).
 */
#include <stdbool.h>
#include <stdio.h>

#include "pruebas/pruebas.h"
#include "configuracion/proximidad.h"
#include "dominio/proximidad.h"
#include "dominio/sonidos.h"
#include "drivers/adc.h"
#include "drivers/botones.h"
#include "drivers/buzzer.h"
#include "drivers/led.h"
#include "drivers/proximidad.h"
#include "hw/tiempo.h"

typedef struct {
    bool     tomada;
    uint16_t adc;       /* ADC crudo medio */
    bool     od;        /* nivel eléctrico de OD */
} toma_t;

/* Promedia unas lecturas con el objeto quieto. */
static toma_t tomar(void)
{
    uint32_t suma = 0;
    int od_alto = 0;
    const int n = 40;

    for (int i = 0; i < n; i++) {
        proximidad_actualizar();
        tiempo_esperar_ms(PROX_PERIODO_MS + 1);
        suma += proximidad_adc_crudo();
        od_alto += proximidad_od_nivel() ? 1 : 0;
    }
    toma_t t = { true, (uint16_t)(suma / n), od_alto * 2 > n };
    return t;
}

static int diferencia(int a, int b) { return a > b ? a - b : b - a; }

static void recomendar(const toma_t *nada, const toma_t *ok, const toma_t *cerca)
{
    printf("\n---- RESULTADO -------------------------------------------------\n");
    printf("ADC crudo: sin objeto=%u  distancia correcta=%u  demasiado cerca=%u\n", nada->adc, ok->adc, cerca->adc);
    printf("Nivel de OD: sin objeto=%d  distancia correcta=%d  demasiado cerca=%d\n", nada->od, ok->od, cerca->od);

    /* Orientación: el "nivel" debe crecer al acercarse. */
    bool invertido = ok->adc < nada->adc;
    int n_nada  = invertido ? 4095 - nada->adc  : nada->adc;
    int n_ok    = invertido ? 4095 - ok->adc    : ok->adc;
    int n_cerca = invertido ? 4095 - cerca->adc : cerca->adc;

    if (diferencia(nada->adc, ok->adc) < 150) {
        printf("AVISO: la salida analógica casi no cambia al acercar el objeto (%d cuentas).\n"
               "       Revisa que OA (no OD) vaya a GP28 y la alimentación a 3V3.\n", diferencia(nada->adc, ok->adc));
    }
    if (!(n_nada < n_ok && n_ok < n_cerca)) {
        printf("AVISO: los tres niveles no están en orden (nada < correcta < cerca): %d, %d, %d.\n"
               "       Repite las tomas con más diferencia de distancia.\n", n_nada, n_ok, n_cerca);
    }

    printf("\nEn configuracion/proximidad.h pon:\n");
    printf("  #define PROX_ANALOGICO_INVERTIDO     %d      /* OA %s al acercar el objeto */\n",
           invertido ? 1 : 0, invertido ? "BAJA" : "SUBE");

    if (nada->od != ok->od) {
        printf("  #define PROX_DIGITAL_ACTIVO_BAJO     %d      /* OD pasa de %d a %d con el objeto cerca */\n",
               ok->od ? 0 : 1, nada->od, ok->od);
    } else {
        printf("  (OD no cambia entre 'sin objeto' y 'distancia correcta': gira el potenciómetro del módulo\n"
               "   hasta que su LED se encienda a esa distancia, o ignora OD y usa solo OA)\n");
    }

    int presencia = n_nada + (n_ok - n_nada) / 4;
    int umbral_ok = n_ok - (n_ok - n_nada) / 3;
    int umbral_cerca = n_ok + (n_cerca - n_ok) / 2;
    printf("  #define PROX_NIVEL_PRESENCIA         %d\n", presencia);
    printf("  #define PROX_NIVEL_OK                %d\n", umbral_ok);
    printf("  #define PROX_NIVEL_CERCA             %d\n", umbral_cerca);
    printf("Recompila y repite con otra muestra (clara y oscura) para comprobar que sirve para las dos.\n");
    printf("----------------------------------------------------------------\n\n");
}

void prueba_proximidad(void)
{
    adc_iniciar();
    botones_iniciar();
    buzzer_iniciar();
    led_iniciar();
    proximidad_iniciar();

    printf("\n== Diagnóstico del IR de proximidad ==\n");
    printf("Configuración actual: OD activo en %s, OA %s al acercar\n",
           PROX_DIGITAL_ACTIVO_BAJO ? "BAJO" : "ALTO", PROX_ANALOGICO_INVERTIDO ? "BAJA" : "SUBE");
    printf("B2 = sin objeto · B3 = distancia correcta · B4 = demasiado cerca · B1 = reiniciar tomas\n\n");

    toma_t nada = { 0 }, ok = { 0 }, cerca = { 0 };
    prox_estado_t anterior = proximidad_estado();
    uint32_t t_info = 0;

    while (true) {
        botones_actualizar();
        buzzer_actualizar();
        proximidad_actualizar();

        if (boton_pulsado(BOTON_1)) {
            nada.tomada = ok.tomada = cerca.tomada = false;
            printf(">> tomas reiniciadas\n");
        }
        if (boton_pulsado(BOTON_2)) {
            nada = tomar();
            printf(">> 'sin objeto': ADC=%u OD=%d\n", nada.adc, nada.od);
            buzzer_reproducir(&SONIDO_CLIC);
        }
        if (boton_pulsado(BOTON_3)) {
            ok = tomar();
            printf(">> 'distancia correcta': ADC=%u OD=%d\n", ok.adc, ok.od);
            buzzer_reproducir(&SONIDO_CLIC);
        }
        if (boton_pulsado(BOTON_4)) {
            cerca = tomar();
            printf(">> 'demasiado cerca': ADC=%u OD=%d\n", cerca.adc, cerca.od);
            buzzer_reproducir(&SONIDO_CLIC);
        }
        if (nada.tomada && ok.tomada && cerca.tomada) {
            recomendar(&nada, &ok, &cerca);
            nada.tomada = ok.tomada = cerca.tomada = false;
        }

        prox_estado_t e = proximidad_estado();
        if (e != anterior) {
            anterior = e;
            printf("estado -> %s\n", prox_texto(e));
            switch (e) {
            case PROX_SIN_OBJETO: led_apagar();        break;
            case PROX_LEJOS:      led_rgb(255, 160, 0); break;
            case PROX_OK:         led_rgb(0, 220, 80);  buzzer_tono(1500, 40); break;
            case PROX_CERCA:      led_rgb(255, 0, 0);   break;
            }
        }

        if ((uint32_t)(tiempo_us() - t_info) >= 250000u) {
            t_info = tiempo_us();
            printf("ADC=%4u nivel=%4u OD=%d (activo=%d)  estado=%s\n", proximidad_adc_crudo(), proximidad_nivel(),
                   proximidad_od_nivel(), proximidad_digital_activo(), prox_texto(e));
        }
        tiempo_esperar_us(500);
    }
}
