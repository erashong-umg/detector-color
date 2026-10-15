/*
 * pines.h — Asignación de pines del detector de color sobre Raspberry Pi Pico W.
 *
 * Ver docs/hardware.md para el razonamiento, el estado de verificación de
 * cada conexión y las decisiones pendientes.
 *
 * Al final del archivo hay comprobaciones en compilación: si se reasigna un
 * pin creando un conflicto (pin repetido, pin reservado por el Wi-Fi, slice
 * PWM compartido entre funciones incompatibles, ADC fuera de GP26..28), el
 * build falla con un mensaje que dice cuál.
 */
#ifndef PINES_H
#define PINES_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Pantalla ST7789 240×240 sin CS — VERIFICADO en Lunar Lander          */
/* BLK cableado fijo a GND (backlight activo en bajo, siempre encendido)*/
/* ------------------------------------------------------------------ */
#define PIN_TFT_DC        16u
#define PIN_TFT_RST       17u
#define PIN_TFT_SCK       18u   /* SPI0 SCK                          */
#define PIN_TFT_MOSI      19u   /* SPI0 TX  ("SDA" en el módulo)     */

/* ------------------------------------------------------------------ */
/* TCS3200                                                             */
/* S0/S1 cableados fijos (escala de frecuencia), OE a GND.             */
/* OUT entra por el canal B de una slice PWM en modo contador de       */
/* flancos: la frecuencia se mide por hardware, sin interrupciones.    */
/* ------------------------------------------------------------------ */
#define PIN_TCS_S2         2u
#define PIN_TCS_S3         3u
#define PIN_TCS_OUT        5u   /* PWM2 B — debe ser impar           */

/* ------------------------------------------------------------------ */
/* Sensores IR de fin de carrera (salida digital: OA u OD, pendiente)  */
/* ------------------------------------------------------------------ */
#define PIN_IR_INICIO      4u
#define PIN_IR_FIN         6u

/* ------------------------------------------------------------------ */
/* Tercer IR, junto al TCS3200: presencia y distancia de la muestra    */
/* Hipótesis: OA = salida analógica (nivel de IR reflejado),           */
/*            OD = salida digital del comparador (umbral del trimmer). */
/* ------------------------------------------------------------------ */
#define PIN_IR_PROX_A     28u   /* ADC2 — distancia relativa          */
#define PIN_IR_PROX_D      1u   /* presencia sí/no                     */

/* ------------------------------------------------------------------ */
/* TB6612FNG — canal A                                                 */
/* ------------------------------------------------------------------ */
#define PIN_MOTOR_STBY     7u   /* 0 = driver deshabilitado (estado seguro) */
#define PIN_MOTOR_PWMA     8u   /* PWM4 A                            */
#define PIN_MOTOR_AIN1     9u
#define PIN_MOTOR_AIN2    10u

/* ------------------------------------------------------------------ */
/* LED RGB de ÁNODO COMÚN — confirmado                                 */
/* Ánodo a +3V3, cada cátodo → resistencia → GPIO. Activo en bajo: se  */
/* enciende con el pin a 0. El driver activa A_INV/B_INV en el PWM     */
/* para que duty 100 % = brillo máximo.                                */
/* R en slice 5, G/B en slice 6: comparten frecuencia, no hay problema */
/* ------------------------------------------------------------------ */
#define PIN_LED_R         11u   /* PWM5 B                            */
#define PIN_LED_G         12u   /* PWM6 A                            */
#define PIN_LED_B         13u   /* PWM6 B                            */
#define LED_ANODO_COMUN   1

/* ------------------------------------------------------------------ */
/* Buzzer pasivo — slice propia porque cambia de frecuencia (TOP/DIV)  */
/* ------------------------------------------------------------------ */
#define PIN_BUZZER        15u   /* PWM7 B (igual que Lunar Lander)   */

/* ------------------------------------------------------------------ */
/* Botones (activos en bajo, pull-up interno + el del módulo)          */
/* ------------------------------------------------------------------ */
#define PIN_BOTON_1       14u   /* Iniciar detección */
#define PIN_BOTON_2       20u   /* Calibrar         */
#define PIN_BOTON_3       21u   /* Posicionar       */
#define PIN_BOTON_4        0u   /* Guardar / menú   */

/* ------------------------------------------------------------------ */
/* Joystick                                                            */
/* ------------------------------------------------------------------ */
#define PIN_JOY_X         26u   /* ADC0 */
#define PIN_JOY_Y         27u   /* ADC1 */
#define PIN_JOY_SW        22u   /* igual que Lunar Lander */

/* ------------------------------------------------------------------ */
/* Libres: ninguno. Los 26 GPIO usables están asignados.              */
/* Reservados por el CYW43 en la Pico W — NO USAR:                     */
/*   GP23 WL_ON, GP24 WL_D, GP25 WL_CS, GP29 WL_CLK / ADC3 (VSYS/3)    */
/* ------------------------------------------------------------------ */
#define PINES_RESERVADOS_PICO_W  ((1ull << 23) | (1ull << 24) | (1ull << 25) | (1ull << 29))

/* ================================================================== */
/* Validación en compilación                                          */
/* ================================================================== */
#define PIN_M(p)        (1ull << (p))
#define PWM_SLICE(p)    (((p) >> 1) & 7u)

/* Todos los pines asignados, como suma y como OR de máscaras.
   Si ningún pin se repite, suma == OR; si alguno se repite, la suma es mayor. */
#define PINES_LISTA(OP) \
    PIN_M(PIN_TFT_DC) OP PIN_M(PIN_TFT_RST) OP PIN_M(PIN_TFT_SCK) OP PIN_M(PIN_TFT_MOSI) OP \
    PIN_M(PIN_TCS_S2) OP PIN_M(PIN_TCS_S3) OP PIN_M(PIN_TCS_OUT) OP                          \
    PIN_M(PIN_IR_INICIO) OP PIN_M(PIN_IR_FIN) OP                                             \
    PIN_M(PIN_IR_PROX_A) OP PIN_M(PIN_IR_PROX_D) OP                                          \
    PIN_M(PIN_MOTOR_STBY) OP PIN_M(PIN_MOTOR_PWMA) OP PIN_M(PIN_MOTOR_AIN1) OP               \
    PIN_M(PIN_MOTOR_AIN2) OP                                                                 \
    PIN_M(PIN_LED_R) OP PIN_M(PIN_LED_G) OP PIN_M(PIN_LED_B) OP PIN_M(PIN_BUZZER) OP         \
    PIN_M(PIN_BOTON_1) OP PIN_M(PIN_BOTON_2) OP PIN_M(PIN_BOTON_3) OP PIN_M(PIN_BOTON_4) OP  \
    PIN_M(PIN_JOY_X) OP PIN_M(PIN_JOY_Y) OP PIN_M(PIN_JOY_SW)

#define PINES_USADOS   (PINES_LISTA(|))

_Static_assert((PINES_LISTA(+)) == PINES_USADOS,
               "pines.h: hay un GPIO asignado a dos funciones");
_Static_assert((PINES_USADOS & PINES_RESERVADOS_PICO_W) == 0,
               "pines.h: se usa un GPIO reservado por el CYW43 (GP23/24/25/29)");
_Static_assert((PINES_USADOS >> 30) == 0,
               "pines.h: el RP2040 solo tiene GP0..GP29");

/* SPI0: SCK solo en GP2/6/18/22, TX solo en GP3/7/19 */
_Static_assert(PIN_TFT_SCK == 2 || PIN_TFT_SCK == 6 || PIN_TFT_SCK == 18 || PIN_TFT_SCK == 22,
               "pines.h: PIN_TFT_SCK no es un SCK de SPI0");
_Static_assert(PIN_TFT_MOSI == 3 || PIN_TFT_MOSI == 7 || PIN_TFT_MOSI == 19,
               "pines.h: PIN_TFT_MOSI no es un TX de SPI0");

/* ADC: solo GP26..GP28 (GP29 lo ocupa el Wi-Fi) */
_Static_assert(PIN_JOY_X >= 26 && PIN_JOY_X <= 28, "pines.h: JX debe ir a GP26..28 (ADC)");
_Static_assert(PIN_JOY_Y >= 26 && PIN_JOY_Y <= 28, "pines.h: JY debe ir a GP26..28 (ADC)");
_Static_assert(PIN_IR_PROX_A >= 26 && PIN_IR_PROX_A <= 28,
               "pines.h: la salida analógica del IR de proximidad debe ir a GP26..28 (ADC)");

/* El contador de flancos del PWM solo lee el canal B (pin impar) */
_Static_assert((PIN_TCS_OUT & 1u) == 1u, "pines.h: PIN_TCS_OUT debe ser impar (canal B)");

/* Slices PWM: cada función con frecuencia propia necesita su slice */
#define SLICES_LED  ((1u << PWM_SLICE(PIN_LED_R)) | (1u << PWM_SLICE(PIN_LED_G)) | \
                     (1u << PWM_SLICE(PIN_LED_B)))
#define SLICE_M(p)  (1u << PWM_SLICE(p))

_Static_assert((SLICE_M(PIN_TCS_OUT) &
                (SLICES_LED | SLICE_M(PIN_BUZZER) | SLICE_M(PIN_MOTOR_PWMA))) == 0,
               "pines.h: la slice del TCS3200 (contador) se comparte con otra salida PWM");
_Static_assert((SLICE_M(PIN_BUZZER) & (SLICES_LED | SLICE_M(PIN_MOTOR_PWMA))) == 0,
               "pines.h: la slice del buzzer se comparte con otra salida PWM");
_Static_assert((SLICE_M(PIN_MOTOR_PWMA) & SLICES_LED) == 0,
               "pines.h: la slice del motor se comparte con el LED RGB");

#endif /* PINES_H */
