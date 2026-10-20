/*
 * prueba_motor.c — Etapa 5 del plan: TB6612FNG, motor y finales de carrera.
 *
 * ¡PRIMERA PRUEBA CON EL MOTOR! Procedimiento recomendado:
 *
 *  0. Con el motor DESCONECTADO del puente (o el carrito sin correa), cargar
 *     el programa y mover a mano el carrito hasta cada final de carrera:
 *     por USB se imprime "INICIO activo/libre" y "FIN activo/libre".
 *     Verificar que se activan en el extremo correcto. Si salen al revés o
 *     siempre activos, revisar FINAL_ACTIVO_BAJO en configuracion/motor.h.
 *  1. Centrar el carrito (ningún final activo) y conectar el motor.
 *  2. B1 arma el puente (LED azul = armado, verde = desarmado, rojo = error).
 *  3. B2 = pulso de prueba hacia el FINAL (200 ms, 30 %), B3 = hacia el INICIO.
 *     Mirar hacia dónde se mueve de verdad: si B2 va hacia el INICIO, poner
 *     MOTOR_SIGNO_HACIA_FIN a −1 y recompilar. NO usar el joystick antes.
 *  4. Joystick en X: mueve el carrito hacia el FINAL (derecha) o el INICIO
 *     (izquierda) hasta CARRO_VEL_MAX_PRUEBA %. Al llegar a un final de
 *     carrera debe frenar de golpe y sonar una alerta.
 *  5. B4 largo (≥ 0,8 s) reinicia un error; B1 desarma en cualquier momento.
 *
 * Seguridad: el carrito para siempre que un final se activa, o si el
 * movimiento dura más de CARRO_TIMEOUT_MS, o si ambos finales se activan.
 * Mantener a mano el corte de la alimentación del motor en esta etapa.
 */
#include <stdbool.h>
#include <stdio.h>

#include "pruebas/pruebas.h"
#include "aplicacion/carro.h"
#include "configuracion/motor.h"
#include "dominio/color.h"
#include "dominio/sonidos.h"
#include "drivers/adc.h"
#include "drivers/botones.h"
#include "drivers/buzzer.h"
#include "drivers/finales.h"
#include "drivers/joystick.h"
#include "hw/tiempo.h"

/* Pulso de prueba para saber en qué sentido gira el motor. Bloquea ≈ 200 ms
   a propósito, pero sigue llamando a carro_actualizar() para que el freno
   y los límites actúen. */
static void pulso(int velocidad, uint32_t ms)
{
    if (!carro_armado() || carro_error() != CARRO_ERROR_NINGUNO) {
        printf("Pulso ignorado: armar primero (B1) / hay un error\n");
        return;
    }
    printf("Pulso %s (%d %%, %lu ms)\n", velocidad > 0 ? "hacia el FINAL" : "hacia el INICIO",
           velocidad, (unsigned long)ms);

    uint32_t t0 = tiempo_us();
    while ((uint32_t)(tiempo_us() - t0) < ms * 1000u) {
        carro_mover(velocidad);
        carro_actualizar();
        buzzer_actualizar();
    }
    carro_parar();
    for (int i = 0; i < 400; i++) {         /* ≈ 400 ms hasta parar con la rampa */
        carro_actualizar();
        tiempo_esperar_us(1000);
    }
}

void prueba_motor(void)
{
    adc_iniciar();
    botones_iniciar();
    joystick_iniciar();
    buzzer_iniciar();
    carro_iniciar();           /* motor en standby, finales leídos */

    printf("\n== Prueba del motor (desarmado) ==\n");
    printf("Finales: INICIO %s, FIN %s (pin crudo %d/%d, activo en %s)\n",
           final_inicio_activo() ? "ACTIVO" : "libre",
           final_fin_activo() ? "ACTIVO" : "libre",
           final_inicio_crudo(), final_fin_crudo(),
           FINAL_ACTIVO_BAJO ? "bajo" : "alto");
    if (carro_error() != CARRO_ERROR_NINGUNO) {
        printf("ERROR: %s\n", carro_error_texto(carro_error()));
        buzzer_reproducir(&SONIDO_ERROR);
    } else {
        buzzer_reproducir(&SONIDO_INICIO);
    }
    printf("B1 armar/desarmar, B2/B3 pulso de prueba, joystick X mover, B4 largo reinicia error\n");

    bool ult_ini = final_inicio_activo(), ult_fin = final_fin_activo();
    carro_error_t ult_error = carro_error();
    bool ult_bloq = false;
    uint32_t t_joy = 0, t_info = 0, t_finales = 0;


    while (true) {
        botones_actualizar();
        buzzer_actualizar();
        carro_actualizar();

        /* --- Cambios de los finales --- */
        if (final_inicio_activo() != ult_ini) {
            ult_ini = final_inicio_activo();
            printf("INICIO %s\n", ult_ini ? "activo" : "libre");
        }
        if (final_fin_activo() != ult_fin) {
            ult_fin = final_fin_activo();
            printf("FIN %s\n", ult_fin ? "activo" : "libre");
        }

        /* --- Errores y bloqueos --- */
        if (carro_error() != ult_error) {
            ult_error = carro_error();
            if (ult_error != CARRO_ERROR_NINGUNO) {
                printf("ERROR: %s -> motor frenado\n", carro_error_texto(ult_error));
                buzzer_reproducir(&SONIDO_ERROR);
            } else {
                printf("Error reiniciado\n");
            }
        }
        if (carro_bloqueado_por_final() && !ult_bloq) {
            printf("Movimiento bloqueado por final de carrera\n");
            buzzer_reproducir(&SONIDO_ALERTA);
        }
        ult_bloq = carro_bloqueado_por_final();

        /* --- Botones --- */
        if (boton_pulsado(BOTON_1)) {
            bool armar = !carro_armado();
            if (carro_armar(armar)) {
                printf("Puente %s\n", armar ? "ARMADO" : "desarmado");
                buzzer_reproducir(armar ? &SONIDO_CONFIRMAR : &SONIDO_CLIC);
            } else {
                printf("No se puede armar: %s\n", carro_error_texto(carro_error()));
                buzzer_reproducir(&SONIDO_ERROR);
            }
        }
        if (boton_pulsado(BOTON_2)) {
            pulso(+30, 200);
        }
        if (boton_pulsado(BOTON_3)) {
            pulso(-30, 200);
        }
        if (boton_largo(BOTON_4)) {
            if (carro_reiniciar_error()) {
                buzzer_reproducir(&SONIDO_CONFIRMAR);
            } else {
                printf("El error persiste: %s\n", carro_error_texto(carro_error()));
                buzzer_reproducir(&SONIDO_ERROR);
            }
        }

        /* --- Joystick (cada 20 ms) --- */
        if ((uint32_t)(tiempo_us() - t_joy) >= 20000u) {
            t_joy = tiempo_us();
            joystick_t j;
            joystick_leer(&j);
            carro_mover(j.x * CARRO_VEL_MAX_PRUEBA / 100);
        }

        /* --- Niveles eléctricos de los finales (cada segundo, con el carrito parado) --- */
        if (carro_velocidad_real() == 0 && (uint32_t)(tiempo_us() - t_finales) >= 1000000u) {
            t_finales = tiempo_us();
            printf("Finales: INICIO nivel=%d (%s)  FIN nivel=%d (%s)\n",
                   final_inicio_crudo(), final_inicio_activo() ? "activo" : "libre",
                   final_fin_crudo(), final_fin_activo() ? "activo" : "libre");
        }

        /* --- Informe mientras se mueve (cada 250 ms) --- */
        if (carro_velocidad_real() != 0 && (uint32_t)(tiempo_us() - t_info) >= 250000u) {
            t_info = tiempo_us();
            printf("velocidad real %d %%\n", carro_velocidad_real());
        }

        tiempo_esperar_us(500);
    }
}
