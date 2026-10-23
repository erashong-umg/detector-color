/*
 * tcs3200_async.h — Adquisición del TCS3200 por interrupción (sin bloquear el bucle principal).
 *
 * Alternativa a tcs3200_medir(), que espera en un bucle ≈ 90 ms. Aquí una alarma del TIMER
 * (ALARM0 → IRQ 0) conduce una pequeña máquina de estados dentro de la ISR:
 *
 *      ESPERA ──(2 ms)──► CUENTA ──(20 ms)──► ESPERA ... (4 canales: R, G, B, claro)
 *
 *   ESPERA: el filtro (S2/S3) ya se cambió; al vencer, la ISR pone a 0 el contador PWM2 y lo habilita.
 *   CUENTA: al vencer, la ISR lo deshabilita, guarda la cuenta y el tiempo exacto de la ventana,
 *           elige el siguiente filtro y rearma la alarma.
 *
 * Cada vuelta completa (≈ 88 ms) publica un juego R-G-B-C que el bucle principal recoge con
 * tcs3200_async_tomar(). La conversión a Hz (división de 64 bits) se hace fuera de la ISR.
 *
 * Registros: TIMER.ALARM0 0x40054010, TIMER.INTR 0x40054034, TIMER.INTE 0x40054038,
 *            NVIC.ISER 0xE000E100, NVIC.IPR0 0xE000E400, PWM2.CTR 0x40050030.
 *
 * La rutina de servicio se llama isr_irq0 (en el SDK, isr_timer_0 es un alias): al definirla aquí,
 * el enlazador la prefiere a la débil de crt0 y queda en la entrada 16 de la tabla de vectores
 * (0x10000100 + 0x40).
 *
 * Pines de depuración (pines.h): PIN_DEP_ISR sube al entrar a la ISR y baja al salir.
 */
#ifndef TCS3200_ASYNC_H
#define TCS3200_ASYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/tcs3200.h"

/* Configura PWM2, ALARM0 y NVIC, y arranca la adquisición continua. */
void tcs3200_async_iniciar(void);

/* Detiene la adquisición (deshabilita la IRQ y el contador). */
void tcs3200_async_detener(void);

/* Si hay un juego nuevo desde la última llamada, lo copia a *f y devuelve true.
   *valido = false si algún canal desbordó el contador de 16 bits. */
bool tcs3200_async_tomar(tcs_frecuencias_t *f, bool *valido);

/* Estadísticas para las pruebas. */
uint32_t tcs3200_async_interrupciones(void);       /* veces que entró la ISR                 */
uint32_t tcs3200_async_juegos(void);               /* juegos R-G-B-C completos publicados     */
uint32_t tcs3200_async_isr_ciclos_max(void);       /* ciclos de la ISR más larga (SysTick)    */

/* PC (dirección de retorno apilada) que tenía el programa al ser interrumpido. Guarda los
   últimos 8: i = 0 es el más reciente. Con arm-none-eabi-addr2line se ve qué función era. */
uint32_t tcs3200_async_pc_interrumpido(uint32_t i);

#endif /* TCS3200_ASYNC_H */
