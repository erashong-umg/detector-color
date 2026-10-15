/*
 * tiempo.h — Base de tiempo sobre el TIMER del RP2040 (contador a 1 MHz).
 *
 * Se lee TIMERAWL (32 bits bajos, sin latch): desborda cada ~71 minutos,
 * pero las restas en uint32_t son correctas a través del desborde.
 * El tick de 1 MHz lo configura el runtime del SDK (watchdog tick = clk_ref/12).
 */
#ifndef TIEMPO_H
#define TIEMPO_H

#include <stdint.h>
#include "rp2040_regs.h"

static inline uint32_t tiempo_us(void)
{
    return RP_TIMER->timerawl;
}

static inline void tiempo_esperar_us(uint32_t us)
{
    uint32_t inicio = tiempo_us();
    while ((uint32_t)(tiempo_us() - inicio) < us) { }
}

static inline void tiempo_esperar_ms(uint32_t ms)
{
    while (ms--) {
        tiempo_esperar_us(1000u);
    }
}

#endif /* TIEMPO_H */
