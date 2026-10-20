/*
 * prueba_eje.c — Etapa 6 del plan: homing y posición por tiempo.
 *
 * REQUISITOS antes de usar esta prueba (etapa 5, prueba_motor.c, ya hecha):
 *   - los finales de carrera se activan en el extremo correcto;
 *   - MOTOR_SIGNO_HACIA_FIN verificado con el pulso de prueba.
 * El sentido invertido se detecta durante el homing, pero solo cuando el
 * carrito choca con el extremo contrario: no sustituye a esa verificación.
 *
 * Controles (LED: verde = desarmado, azul = armado, amarillo = en movimiento,
 * rojo = error):
 *   B1  arma / desarma el puente
 *   B2  REFERENCIAR: busca el inicio, calibra el recorrido y vuelve al inicio
 *   B3  siguiente posición de una lista (0, 25, 50, 75, 100 % y vuelta)
 *   B4  paseo de repetibilidad: 8 viajes entre 60 mm y 290 mm
 *   B4 largo  reinicia un error
 *   Joystick X  jog manual (solo con el eje parado)
 *
 * Por USB se imprime cada cambio de estado, la duración de la calibración y,
 * al llegar a cada destino, la posición estimada. Para medir la exactitud
 * real: marcar con un lápiz dónde para el cursor en cada destino y medir con
 * regla. Con el paseo, la dispersión entre viajes al mismo punto es la
 * repetibilidad; la diferencia con la regla es el error del modelo.
 */
#include <stdbool.h>
#include <stdio.h>

#include "pruebas/pruebas.h"
#include "aplicacion/carro.h"
#include "aplicacion/eje.h"
#include "configuracion/eje.h"
#include "configuracion/motor.h"
#include "dominio/sonidos.h"
#include "drivers/adc.h"
#include "drivers/botones.h"
#include "drivers/buzzer.h"
#include "drivers/joystick.h"
#include "hw/tiempo.h"

#define PASEO_A_UM   60000
#define PASEO_B_UM   290000
#define PASEO_VIAJES 8

static void imprimir_pos(const char *etiqueta)
{
    int32_t um = eje_posicion_um();
    if (um < 0) {
        printf("%s: posicion desconocida (sin calibrar)\n", etiqueta);
    } else {
        printf("%s: %ld.%01ld mm\n", etiqueta, (long)(um / 1000), (long)((um % 1000) / 100));
    }
}

void prueba_eje(void)
{
    adc_iniciar();
    botones_iniciar();
    joystick_iniciar();
    buzzer_iniciar();
    carro_iniciar();
    eje_iniciar();

    printf("\n== Prueba del eje (desarmado) ==\n");
    printf("B1 armar, B2 referenciar, B3 lista de posiciones, B4 paseo, B4 largo reinicia error\n");
    if (carro_error() != CARRO_ERROR_NINGUNO) {
        printf("ERROR: %s\n", carro_error_texto(carro_error()));
        buzzer_reproducir(&SONIDO_ERROR);
    } else {
        buzzer_reproducir(&SONIDO_INICIO);
    }

    static const int32_t lista[] = { 0, 87500, 175000, 262500, 350000, 175000 };
    unsigned i_lista = 0;
    int paseo_restante = 0;
    bool paseo_hacia_b = true;

    eje_estado_t ult_estado = eje_estado();
    uint32_t t_joy = 0, t_info = 0;

    while (true) {
        botones_actualizar();
        buzzer_actualizar();
        eje_actualizar();

        /* --- Cambios de estado --- */
        if (eje_estado() != ult_estado) {
            printf("estado: %s -> %s\n", eje_estado_texto(ult_estado), eje_estado_texto(eje_estado()));

            if (ult_estado == EJE_CALIBRANDO && eje_calibrado()) {
                uint32_t ms = eje_recorrido_ms();
                printf("Calibrado: recorrido de %lu ms a %d %% -> %lu mm/s medio\n",
                       (unsigned long)ms, EJE_VEL_VIAJE,
                       (unsigned long)((uint64_t)(EJE_LONGITUD_UM / 1000) * 1000u / ms));
            }
            if (eje_estado() == EJE_LISTO) {
                imprimir_pos("llegada");
                buzzer_reproducir(&SONIDO_CONFIRMAR);
            }
            if (eje_estado() == EJE_ERROR) {
                printf("ERROR: %s\n", eje_error_texto(eje_error()));
                buzzer_reproducir(&SONIDO_ERROR);
                paseo_restante = 0;
            }
            ult_estado = eje_estado();
        }

        /* --- Paseo de repetibilidad: encadena viajes al llegar --- */
        if (paseo_restante > 0 && eje_estado() == EJE_LISTO) {
            paseo_restante--;
            eje_ir_a(paseo_hacia_b ? PASEO_B_UM : PASEO_A_UM);
            paseo_hacia_b = !paseo_hacia_b;
            if (paseo_restante == 0) {
                printf("Paseo terminado\n");
            }
        }

        /* --- Botones --- */
        if (boton_pulsado(BOTON_1)) {
            bool armar = !carro_armado();
            if (carro_armar(armar)) {
                printf("Puente %s\n", armar ? "ARMADO" : "desarmado");
                buzzer_reproducir(armar ? &SONIDO_CONFIRMAR : &SONIDO_CLIC);
                if (!armar) {
                    eje_parar();
                    paseo_restante = 0;
                }
            } else {
                printf("No se puede armar: %s\n", carro_error_texto(carro_error()));
                buzzer_reproducir(&SONIDO_ERROR);
            }
        }
        if (boton_pulsado(BOTON_2)) {
            if (eje_referenciar()) {
                printf("Referenciando: buscando el inicio...\n");
                paseo_restante = 0;
            } else {
                printf("No se puede referenciar: puente %s, error del carro: %s, error del eje: %s\n",
                       carro_armado() ? "ARMADO" : "desarmado (armar con B1)",
                       carro_error_texto(carro_error()), eje_error_texto(eje_error()));
                buzzer_reproducir(&SONIDO_ALERTA);
            }
        }
        if (boton_pulsado(BOTON_3)) {
            int32_t destino = lista[i_lista];
            i_lista = (i_lista + 1) % (sizeof lista / sizeof lista[0]);
            if (eje_ir_a(destino)) {
                printf("Yendo a %ld mm\n", (long)(destino / 1000));
            } else {
                printf("No se puede mover: referenciar primero (B2)\n");
                buzzer_reproducir(&SONIDO_ALERTA);
            }
        }
        if (boton_pulsado(BOTON_4)) {
            if (eje_calibrado() && eje_estado() == EJE_LISTO) {
                paseo_restante = PASEO_VIAJES;
                paseo_hacia_b = true;
                printf("Paseo de repetibilidad: %d viajes entre %d y %d mm\n",
                       PASEO_VIAJES, PASEO_A_UM / 1000, PASEO_B_UM / 1000);
            } else {
                printf("El paseo necesita el eje calibrado y parado\n");
                buzzer_reproducir(&SONIDO_ALERTA);
            }
        }
        if (boton_largo(BOTON_4)) {
            paseo_restante = 0;
            if (eje_reiniciar()) {
                printf("Error reiniciado: referenciar de nuevo (B2)\n");
                buzzer_reproducir(&SONIDO_CONFIRMAR);
            } else {
                printf("El error persiste: %s\n", carro_error_texto(carro_error()));
                buzzer_reproducir(&SONIDO_ERROR);
            }
        }

        /* --- Joystick: jog manual (cada 20 ms) --- */
        if ((uint32_t)(tiempo_us() - t_joy) >= 20000u) {
            t_joy = tiempo_us();
            joystick_t j;
            joystick_leer(&j);
            eje_manual(j.x * CARRO_VEL_MAX_PRUEBA / 100);
        }

        /* --- Informe mientras se mueve (cada 250 ms) --- */
        if (carro_velocidad_real() != 0 && (uint32_t)(tiempo_us() - t_info) >= 250000u) {
            t_info = tiempo_us();
            int32_t um = eje_posicion_um();
            if (um >= 0) {
                printf("  pos %ld.%01ld mm  vel %d %%\n", (long)(um / 1000),
                       (long)((um % 1000) / 100), carro_velocidad_real());
            } else {
                printf("  vel %d %%\n", carro_velocidad_real());
            }
        }

        tiempo_esperar_us(500);
    }
}
