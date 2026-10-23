/*
 * ciclos.h — Cronómetro de ciclos de CPU con SysTick (PPB del Cortex-M0+).
 *
 * SysTick es un contador de 24 bits que DECRECE una unidad por ciclo de clk_sys
 * (CLKSOURCE = 1). Se mide como (inicio − fin) & 0xFFFFFF. Con clk_sys = 125 MHz
 * desborda cada 16 777 216 ciclos ≈ 134 ms: los bloques medidos deben ser más
 * cortos que eso. COUNTFLAG (bit 16 de CSR) avisa si dio la vuelta.
 *
 *   SYST_CSR 0xE000E010   SYST_RVR 0xE000E014   SYST_CVR 0xE000E018
 *
 * No usa la interrupción de SysTick (TICKINT = 0).
 */
#ifndef CICLOS_H
#define CICLOS_H

#include <stdbool.h>
#include <stdint.h>
#include "rp2040_regs.h"

#define CICLOS_CSR_ENABLE     (1u << 0)
#define CICLOS_CSR_CLKSOURCE  (1u << 2)    /* 1 = reloj del procesador */
#define CICLOS_CSR_COUNTFLAG  (1u << 16)   /* 1 = dio la vuelta desde la última lectura de CSR */
#define CICLOS_MASCARA        0x00FFFFFFu

static inline void ciclos_iniciar(void)
{
    RP_SYSTICK->csr = 0;
    RP_SYSTICK->rvr = CICLOS_MASCARA;
    RP_SYSTICK->cvr = 0;                   /* cualquier escritura lo pone a 0 */
    RP_SYSTICK->csr = CICLOS_CSR_ENABLE | CICLOS_CSR_CLKSOURCE;
}

static inline uint32_t ciclos_leer(void)
{
    return RP_SYSTICK->cvr;
}

/* Ciclos transcurridos entre dos lecturas (la cuenta es descendente). */
static inline uint32_t ciclos_entre(uint32_t inicio, uint32_t fin)
{
    return (inicio - fin) & CICLOS_MASCARA;
}

/* Lee CSR: borra COUNTFLAG. Llamar antes de medir y mirar el resultado después. */
static inline bool ciclos_desbordo(void)
{
    return (RP_SYSTICK->csr & CICLOS_CSR_COUNTFLAG) != 0;
}

/* Enmascara las interrupciones (PRIMASK = 1) y devuelve el PRIMASK anterior. */
static inline uint32_t ciclos_irq_apagar(void)
{
    uint32_t anterior;
    __asm__ volatile ("mrs %0, primask\n\tcpsid i" : "=r"(anterior) : : "memory");
    return anterior;
}

static inline void ciclos_irq_restaurar(uint32_t anterior)
{
    if ((anterior & 1u) == 0) {
        __asm__ volatile ("cpsie i" : : : "memory");
    }
}

#endif /* CICLOS_H */
