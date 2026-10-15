/*
 * rp2040_regs.h — Estructuras de registros de los periféricos del RP2040.
 *
 * Cada struct reproduce el bloque de registros del datasheet campo a campo;
 * se superpone a la dirección base del periférico con un puntero volátil.
 * Los _Static_assert verifican en compilación que cada offset coincide con
 * el datasheet: si alguien desalinea un campo, el build falla.
 *
 * Por cada periférico con alias atómicos se definen cuatro vistas:
 *   RP_X      lectura/escritura normal
 *   RP_X_SET  escribir 1 pone bits a 1
 *   RP_X_CLR  escribir 1 pone bits a 0
 *   RP_X_XOR  escribir 1 invierte bits
 *
 * Solo se incluyen los periféricos que usa el proyecto:
 *   RESETS, IO_BANK0, PADS_BANK0, SIO, SPI, PWM, ADC, TIMER, NVIC, SysTick.
 */
#ifndef RP2040_REGS_H
#define RP2040_REGS_H

#include <stdint.h>
#include <stddef.h>
#include "rp2040_mapa.h"

#define RP_REG  volatile uint32_t        /* registro de lectura/escritura */
#define RP_RO   const volatile uint32_t  /* registro de solo lectura      */

#define RP_OFFSET(tipo, campo, esperado) \
    _Static_assert(offsetof(tipo, campo) == (esperado), \
                   #tipo "." #campo " no está en " #esperado)

#define RP_NUM_GPIO 30u

/* ================================================================== */
/* RESETS (§2.14) — mantiene periféricos en reset hasta liberarlos     */
/* ================================================================== */
typedef struct {
    RP_REG reset;       /* 0x00: 1 = periférico en reset           */
    RP_REG wdsel;       /* 0x04: qué resetea el watchdog           */
    RP_RO  reset_done;  /* 0x08: 1 = periférico ya salió del reset */
} rp_resets_t;
RP_OFFSET(rp_resets_t, reset_done, 0x08);

#define RP_RESETS       ((rp_resets_t *)RP_RESETS_BASE)
#define RP_RESETS_SET   ((rp_resets_t *)(RP_RESETS_BASE + RP_ALIAS_SET))
#define RP_RESETS_CLR   ((rp_resets_t *)(RP_RESETS_BASE + RP_ALIAS_CLR))

/* Bits de RESET / RESET_DONE */
#define RP_RESET_ADC        (1u << 0)
#define RP_RESET_DMA        (1u << 2)
#define RP_RESET_I2C0       (1u << 3)
#define RP_RESET_I2C1       (1u << 4)
#define RP_RESET_IO_BANK0   (1u << 5)
#define RP_RESET_PADS_BANK0 (1u << 8)
#define RP_RESET_PIO0       (1u << 10)
#define RP_RESET_PIO1       (1u << 11)
#define RP_RESET_PWM        (1u << 14)
#define RP_RESET_SPI0       (1u << 16)
#define RP_RESET_SPI1       (1u << 17)
#define RP_RESET_TIMER      (1u << 21)
#define RP_RESET_UART0      (1u << 22)
#define RP_RESET_UART1      (1u << 23)

/* ================================================================== */
/* IO_BANK0 (§2.19.6.1) — multiplexor de función e IRQ por pin         */
/* ================================================================== */
typedef struct {
    RP_RO  status;      /* +0x0: estado real de la señal en el pin */
    RP_REG ctrl;        /* +0x4: FUNCSEL y overrides               */
} rp_io_gpio_t;

typedef struct {
    RP_REG inte[4];     /* habilitar IRQ                    */
    RP_REG intf[4];     /* forzar IRQ                       */
    RP_RO  ints[4];     /* estado tras máscara y forzado    */
} rp_io_irq_ctrl_t;

typedef struct {
    rp_io_gpio_t     gpio[RP_NUM_GPIO]; /* 0x000 - 0x0EC                   */
    RP_REG           intr[4];           /* 0x0F0: flancos crudos (W1C)      */
    rp_io_irq_ctrl_t proc[2];           /* 0x100: núcleo 0, 0x130: núcleo 1 */
    rp_io_irq_ctrl_t dormant_wake;      /* 0x160                            */
} rp_io_bank0_t;
RP_OFFSET(rp_io_bank0_t, gpio[1].ctrl,       0x00C);
RP_OFFSET(rp_io_bank0_t, intr,               0x0F0);
RP_OFFSET(rp_io_bank0_t, proc[0].inte,       0x100);
RP_OFFSET(rp_io_bank0_t, proc[0].ints,       0x120);
RP_OFFSET(rp_io_bank0_t, proc[1].inte,       0x130);
RP_OFFSET(rp_io_bank0_t, dormant_wake.inte,  0x160);

#define RP_IO_BANK0     ((rp_io_bank0_t *)RP_IO_BANK0_BASE)
#define RP_IO_BANK0_SET ((rp_io_bank0_t *)(RP_IO_BANK0_BASE + RP_ALIAS_SET))
#define RP_IO_BANK0_CLR ((rp_io_bank0_t *)(RP_IO_BANK0_BASE + RP_ALIAS_CLR))

/* GPIOx_CTRL */
#define RP_IO_CTRL_FUNCSEL_LSB   0u
#define RP_IO_CTRL_FUNCSEL_MASK  (0x1Fu << 0)

/* Valores de FUNCSEL (§2.19.2) */
typedef enum {
    RP_FUNC_XIP  = 0,
    RP_FUNC_SPI  = 1,
    RP_FUNC_UART = 2,
    RP_FUNC_I2C  = 3,
    RP_FUNC_PWM  = 4,
    RP_FUNC_SIO  = 5,   /* GPIO por software (registros SIO) */
    RP_FUNC_PIO0 = 6,
    RP_FUNC_PIO1 = 7,
    RP_FUNC_GPCK = 8,
    RP_FUNC_USB  = 9,
    RP_FUNC_NULL = 0x1F /* pin desconectado de toda función  */
} rp_funcion_t;

/* INTRx: 4 bits por pin, 8 pines por registro */
#define RP_IO_EV_NIVEL_BAJO   0x1u
#define RP_IO_EV_NIVEL_ALTO   0x2u
#define RP_IO_EV_FLANCO_BAJA  0x4u
#define RP_IO_EV_FLANCO_SUBE  0x8u
#define RP_IO_EV_REG(pin)     ((pin) >> 3)
#define RP_IO_EV_DESP(pin)    (((pin) & 7u) * 4u)

/* ================================================================== */
/* PADS_BANK0 (§2.19.6.3) — parte eléctrica del pin                    */
/* ================================================================== */
typedef struct {
    RP_REG voltage_select;      /* 0x00: 0 = 3V3, 1 = 1V8           */
    RP_REG gpio[RP_NUM_GPIO];   /* 0x04 - 0x78                       */
    RP_REG swclk;               /* 0x7C                              */
    RP_REG swd;                 /* 0x80                              */
} rp_pads_bank0_t;
RP_OFFSET(rp_pads_bank0_t, gpio[0],  0x04);
RP_OFFSET(rp_pads_bank0_t, gpio[29], 0x78);
RP_OFFSET(rp_pads_bank0_t, swd,      0x80);

#define RP_PADS_BANK0     ((rp_pads_bank0_t *)RP_PADS_BANK0_BASE)
#define RP_PADS_BANK0_SET ((rp_pads_bank0_t *)(RP_PADS_BANK0_BASE + RP_ALIAS_SET))
#define RP_PADS_BANK0_CLR ((rp_pads_bank0_t *)(RP_PADS_BANK0_BASE + RP_ALIAS_CLR))

#define RP_PAD_SLEWFAST   (1u << 0)
#define RP_PAD_SCHMITT    (1u << 1)
#define RP_PAD_PDE        (1u << 2)   /* pull-down */
#define RP_PAD_PUE        (1u << 3)   /* pull-up   */
#define RP_PAD_DRIVE_MASK (3u << 4)   /* 0=2mA 1=4mA 2=8mA 3=12mA */
#define RP_PAD_IE         (1u << 6)   /* habilita entrada  */
#define RP_PAD_OD         (1u << 7)   /* deshabilita salida */

/* ================================================================== */
/* SIO (§2.3.1.7) — GPIO de un ciclo. Sin alias atómicos: trae sus     */
/* propios registros _SET/_CLR/_XOR.                                   */
/* ================================================================== */
typedef struct {
    RP_RO  cpuid;           /* 0x00: 0 = núcleo 0, 1 = núcleo 1 */
    RP_RO  gpio_in;         /* 0x04                              */
    RP_RO  gpio_hi_in;      /* 0x08: pines QSPI                   */
    uint32_t _reservado0;   /* 0x0C                              */
    RP_REG gpio_out;        /* 0x10                              */
    RP_REG gpio_out_set;    /* 0x14                              */
    RP_REG gpio_out_clr;    /* 0x18                              */
    RP_REG gpio_out_xor;    /* 0x1C                              */
    RP_REG gpio_oe;         /* 0x20: 1 = pin como salida          */
    RP_REG gpio_oe_set;     /* 0x24                              */
    RP_REG gpio_oe_clr;     /* 0x28                              */
    RP_REG gpio_oe_xor;     /* 0x2C                              */
    RP_REG gpio_hi_out;     /* 0x30                              */
    RP_REG gpio_hi_out_set; /* 0x34                              */
    RP_REG gpio_hi_out_clr; /* 0x38                              */
    RP_REG gpio_hi_out_xor; /* 0x3C                              */
    RP_REG gpio_hi_oe;      /* 0x40                              */
    RP_REG gpio_hi_oe_set;  /* 0x44                              */
    RP_REG gpio_hi_oe_clr;  /* 0x48                              */
    RP_REG gpio_hi_oe_xor;  /* 0x4C                              */
    RP_RO  fifo_st;         /* 0x50: FIFO entre núcleos           */
    RP_REG fifo_wr;         /* 0x54                              */
    RP_RO  fifo_rd;         /* 0x58                              */
    RP_RO  spinlock_st;     /* 0x5C                              */
    RP_REG div_udividend;   /* 0x60: divisor hardware             */
    RP_REG div_udivisor;    /* 0x64                              */
    RP_REG div_sdividend;   /* 0x68                              */
    RP_REG div_sdivisor;    /* 0x6C                              */
    RP_REG div_quotient;    /* 0x70                              */
    RP_REG div_remainder;   /* 0x74                              */
    RP_RO  div_csr;         /* 0x78                              */
} rp_sio_t;
RP_OFFSET(rp_sio_t, gpio_out,    0x10);
RP_OFFSET(rp_sio_t, gpio_oe,     0x20);
RP_OFFSET(rp_sio_t, gpio_hi_oe,  0x40);
RP_OFFSET(rp_sio_t, fifo_st,     0x50);
RP_OFFSET(rp_sio_t, div_csr,     0x78);

#define RP_SIO ((rp_sio_t *)RP_SIO_BASE)

/* ================================================================== */
/* SPI — ARM PrimeCell PL022 (§4.4)                                    */
/* ================================================================== */
typedef struct {
    RP_REG cr0;     /* 0x00: DSS, FRF, SPO, SPH, SCR    */
    RP_REG cr1;     /* 0x04: SSE (habilitar), MS         */
    RP_REG dr;      /* 0x08: dato TX/RX                  */
    RP_RO  sr;      /* 0x0C: estado de las FIFO          */
    RP_REG cpsr;    /* 0x10: prescaler par 2..254         */
    RP_REG imsc;    /* 0x14                              */
    RP_RO  ris;     /* 0x18                              */
    RP_RO  mis;     /* 0x1C                              */
    RP_REG icr;     /* 0x20                              */
    RP_REG dmacr;   /* 0x24                              */
} rp_spi_t;
RP_OFFSET(rp_spi_t, sr,    0x0C);
RP_OFFSET(rp_spi_t, dmacr, 0x24);

#define RP_SPI0     ((rp_spi_t *)RP_SPI0_BASE)
#define RP_SPI1     ((rp_spi_t *)RP_SPI1_BASE)

#define RP_SPI_CR0_DSS(bits)  ((uint32_t)((bits) - 1u) & 0xFu) /* 4..16 bits */
#define RP_SPI_CR0_SPO        (1u << 6)   /* CPOL */
#define RP_SPI_CR0_SPH        (1u << 7)   /* CPHA */
#define RP_SPI_CR0_SCR(n)     (((uint32_t)(n) & 0xFFu) << 8)
#define RP_SPI_CR1_SSE        (1u << 1)
#define RP_SPI_CR1_MS         (1u << 2)   /* 1 = esclavo */
#define RP_SPI_SR_TFE         (1u << 0)   /* FIFO TX vacía     */
#define RP_SPI_SR_TNF         (1u << 1)   /* FIFO TX no llena  */
#define RP_SPI_SR_RNE         (1u << 2)   /* FIFO RX no vacía  */
#define RP_SPI_SR_BSY         (1u << 4)
#define RP_SPI_ICR_RORIC      (1u << 0)   /* limpia el desborde de RX */

/* ================================================================== */
/* PWM (§4.5) — 8 slices, 2 canales (A pin par, B pin impar)           */
/* El canal B también sirve como ENTRADA: con DIVMODE=flanco el        */
/* contador cuenta flancos del pin → medidor de frecuencia (TCS3200).  */
/* ================================================================== */
typedef struct {
    RP_REG csr;     /* +0x00 */
    RP_REG div;     /* +0x04: INT[11:4] . FRAC[3:0] */
    RP_REG ctr;     /* +0x08 */
    RP_REG cc;      /* +0x0C: A[15:0], B[31:16]     */
    RP_REG top;     /* +0x10 */
} rp_pwm_slice_t;

typedef struct {
    rp_pwm_slice_t slice[8];  /* 0x00 - 0x9C */
    RP_REG en;                /* 0xA0: habilita varias slices a la vez */
    RP_REG intr;              /* 0xA4 */
    RP_REG inte;              /* 0xA8 */
    RP_REG intf;              /* 0xAC */
    RP_RO  ints;              /* 0xB0 */
} rp_pwm_t;
RP_OFFSET(rp_pwm_t, slice[1].csr, 0x14);
RP_OFFSET(rp_pwm_t, slice[7].top, 0x9C);
RP_OFFSET(rp_pwm_t, en,           0xA0);
RP_OFFSET(rp_pwm_t, ints,         0xB0);

#define RP_PWM      ((rp_pwm_t *)RP_PWM_BASE)
#define RP_PWM_SET  ((rp_pwm_t *)(RP_PWM_BASE + RP_ALIAS_SET))
#define RP_PWM_CLR  ((rp_pwm_t *)(RP_PWM_BASE + RP_ALIAS_CLR))

#define RP_PWM_SLICE(pin)      (((pin) >> 1) & 7u)
#define RP_PWM_CANAL_B(pin)    ((pin) & 1u)

#define RP_PWM_CSR_EN          (1u << 0)
#define RP_PWM_CSR_PH_CORRECT  (1u << 1)
#define RP_PWM_CSR_A_INV       (1u << 2)
#define RP_PWM_CSR_B_INV       (1u << 3)
#define RP_PWM_CSR_DIVMODE(m)  (((uint32_t)(m) & 3u) << 4)
#define RP_PWM_DIVMODE_LIBRE        0u  /* PWM normal             */
#define RP_PWM_DIVMODE_NIVEL_B      1u  /* cuenta mientras B = 1   */
#define RP_PWM_DIVMODE_FLANCO_SUBE  2u  /* cuenta flancos ↑ en B   */
#define RP_PWM_DIVMODE_FLANCO_BAJA  3u  /* cuenta flancos ↓ en B   */
#define RP_PWM_DIV(entero, frac) ((((uint32_t)(entero) & 0xFFu) << 4) | ((uint32_t)(frac) & 0xFu))

/* ================================================================== */
/* ADC (§4.9) — 12 bits, AIN0..3 = GPIO26..29, AIN4 = sensor temp.     */
/* ================================================================== */
typedef struct {
    RP_REG cs;      /* 0x00 */
    RP_RO  result;  /* 0x04 */
    RP_REG fcs;     /* 0x08 */
    RP_RO  fifo;    /* 0x0C */
    RP_REG div;     /* 0x10 */
    RP_RO  intr;    /* 0x14 */
    RP_REG inte;    /* 0x18 */
    RP_REG intf;    /* 0x1C */
    RP_RO  ints;    /* 0x20 */
} rp_adc_t;
RP_OFFSET(rp_adc_t, div,  0x10);
RP_OFFSET(rp_adc_t, ints, 0x20);

#define RP_ADC      ((rp_adc_t *)RP_ADC_BASE)
#define RP_ADC_SET  ((rp_adc_t *)(RP_ADC_BASE + RP_ALIAS_SET))
#define RP_ADC_CLR  ((rp_adc_t *)(RP_ADC_BASE + RP_ALIAS_CLR))

#define RP_ADC_CS_EN          (1u << 0)
#define RP_ADC_CS_TS_EN       (1u << 1)
#define RP_ADC_CS_START_ONCE  (1u << 2)
#define RP_ADC_CS_READY       (1u << 8)
#define RP_ADC_CS_ERR         (1u << 9)
#define RP_ADC_CS_AINSEL_LSB  12u
#define RP_ADC_CS_AINSEL_MASK (7u << 12)
#define RP_ADC_PRIMER_PIN     26u   /* AIN0 */

/* ================================================================== */
/* TIMER (§4.6) — contador libre de 64 bits a 1 MHz                    */
/* ================================================================== */
typedef struct {
    RP_REG timehw;      /* 0x00 */
    RP_REG timelw;      /* 0x04 */
    RP_RO  timehr;      /* 0x08: leer DESPUÉS de timelr (queda latcheado) */
    RP_RO  timelr;      /* 0x0C */
    RP_REG alarm[4];    /* 0x10 - 0x1C */
    RP_REG armed;       /* 0x20 */
    RP_RO  timerawh;    /* 0x24: sin latch */
    RP_RO  timerawl;    /* 0x28: µs, 32 bits (desborda cada ~71 min) */
    RP_REG dbgpause;    /* 0x2C */
    RP_REG pause;       /* 0x30 */
    RP_REG intr;        /* 0x34 */
    RP_REG inte;        /* 0x38 */
    RP_REG intf;        /* 0x3C */
    RP_RO  ints;        /* 0x40 */
} rp_timer_t;
RP_OFFSET(rp_timer_t, alarm[0], 0x10);
RP_OFFSET(rp_timer_t, timerawl, 0x28);
RP_OFFSET(rp_timer_t, ints,     0x40);

#define RP_TIMER     ((rp_timer_t *)RP_TIMER_BASE)
#define RP_TIMER_SET ((rp_timer_t *)(RP_TIMER_BASE + RP_ALIAS_SET))
#define RP_TIMER_CLR ((rp_timer_t *)(RP_TIMER_BASE + RP_ALIAS_CLR))

/* ================================================================== */
/* PPB del Cortex-M0+: SysTick y NVIC (ARMv6-M ARM, §B3)               */
/* ================================================================== */
typedef struct {
    RP_REG csr;     /* 0x0: ENABLE, TICKINT, CLKSOURCE, COUNTFLAG */
    RP_REG rvr;     /* 0x4: valor de recarga (24 bits)            */
    RP_REG cvr;     /* 0x8: valor actual                          */
    RP_RO  calib;   /* 0xC                                        */
} rp_systick_t;
#define RP_SYSTICK ((rp_systick_t *)RP_SYSTICK_BASE)

typedef struct {
    RP_REG   iser;              /* 0x000: habilitar IRQ (escribir 1)   */
    uint32_t _reservado0[31];
    RP_REG   icer;              /* 0x080: deshabilitar IRQ             */
    uint32_t _reservado1[31];
    RP_REG   ispr;              /* 0x100: marcar pendiente             */
    uint32_t _reservado2[31];
    RP_REG   icpr;              /* 0x180: limpiar pendiente            */
    uint32_t _reservado3[95];
    RP_REG   ipr[8];            /* 0x300: prioridad, 2 bits útiles/IRQ */
} rp_nvic_t;
RP_OFFSET(rp_nvic_t, icer,   0x080);
RP_OFFSET(rp_nvic_t, icpr,   0x180);
RP_OFFSET(rp_nvic_t, ipr[0], 0x300);
#define RP_NVIC ((rp_nvic_t *)RP_NVIC_BASE)

/* Números de IRQ (§2.3.2) usados por el proyecto */
#define RP_IRQ_TIMER_0      0u
#define RP_IRQ_TIMER_1      1u
#define RP_IRQ_TIMER_2      2u
#define RP_IRQ_TIMER_3      3u
#define RP_IRQ_PWM_WRAP     4u
#define RP_IRQ_IO_BANK0     13u
#define RP_IRQ_SPI0         18u
#define RP_IRQ_SPI1         19u
#define RP_IRQ_ADC_FIFO     22u

/* Utilidad: espera a que un periférico salga del reset */
static inline void rp_liberar_reset(uint32_t mascara)
{
    RP_RESETS_CLR->reset = mascara;
    while ((RP_RESETS->reset_done & mascara) != mascara) { }
}

#endif /* RP2040_REGS_H */
