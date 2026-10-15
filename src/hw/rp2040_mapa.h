/*
 * rp2040_mapa.h — Mapa de memoria del RP2040.
 *
 * Fuente: RP2040 Datasheet, §2.2 "Address Map" y §2.8 "Bootrom".
 * Este mapa es del CHIP, no de la placa: vale igual para Pico y Pico W.
 * (Un RP2350 / Pico 2 W tiene otro mapa y NO es compatible con este archivo.)
 *
 * Todos los nombres llevan prefijo RP_ / rp_ para no chocar con los del
 * Pico SDK (hardware/regs/addressmap.h define SIO_BASE, etc.).
 *
 *   0x00000000  ROM  (16 KB, bootrom de solo lectura)
 *   0x10000000  XIP  (flash QSPI externa mapeada, hasta 16 MB)
 *   0x20000000  SRAM (264 KB: 256 KB entrelazados + SRAM4 + SRAM5)
 *   0x40000000  Periféricos APB
 *   0x50000000  Periféricos AHB-Lite (DMA, USB, PIO)
 *   0xD0000000  SIO  (GPIO de un ciclo, FIFO entre núcleos, divisor)
 *   0xE0000000  PPB  (periféricos privados del Cortex-M0+: NVIC, SysTick)
 */
#ifndef RP2040_MAPA_H
#define RP2040_MAPA_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Regiones de memoria                                                 */
/* ------------------------------------------------------------------ */

/* ROM */
#define RP_ROM_BASE             0x00000000u
#define RP_ROM_TAMANO           (16u * 1024u)

/* Flash externa vía XIP (eXecute In Place) */
#define RP_XIP_BASE             0x10000000u   /* con caché            */
#define RP_XIP_NOALLOC_BASE     0x11000000u
#define RP_XIP_NOCACHE_BASE     0x12000000u
#define RP_XIP_NOCACHE_NOALLOC  0x13000000u   /* acceso directo        */
#define RP_XIP_CTRL_BASE        0x14000000u
#define RP_XIP_SRAM_BASE        0x15000000u   /* caché XIP como SRAM   */
#define RP_XIP_SSI_BASE         0x18000000u

/* SRAM */
#define RP_SRAM_BASE            0x20000000u   /* SRAM0-3 entrelazadas  */
#define RP_SRAM_ENTRELAZADA_FIN 0x20040000u   /* 256 KB                */
#define RP_SRAM4_BASE           0x20040000u   /* 4 KB (pila núcleo 0)  */
#define RP_SRAM5_BASE           0x20041000u   /* 4 KB (pila núcleo 1)  */
#define RP_SRAM_FIN             0x20042000u
#define RP_SRAM_TAMANO          (RP_SRAM_FIN - RP_SRAM_BASE)  /* 264 KB */
/* Alias sin entrelazar: cada banco de 64 KB contiguo */
#define RP_SRAM0_BASE           0x21000000u
#define RP_SRAM1_BASE           0x21010000u
#define RP_SRAM2_BASE           0x21020000u
#define RP_SRAM3_BASE           0x21030000u

/* ------------------------------------------------------------------ */
/* Periféricos APB                                                     */
/* ------------------------------------------------------------------ */
#define RP_SYSINFO_BASE         0x40000000u
#define RP_SYSCFG_BASE          0x40004000u
#define RP_CLOCKS_BASE          0x40008000u
#define RP_RESETS_BASE          0x4000C000u
#define RP_PSM_BASE             0x40010000u
#define RP_IO_BANK0_BASE        0x40014000u
#define RP_IO_QSPI_BASE         0x40018000u
#define RP_PADS_BANK0_BASE      0x4001C000u
#define RP_PADS_QSPI_BASE       0x40020000u
#define RP_XOSC_BASE            0x40024000u
#define RP_PLL_SYS_BASE         0x40028000u
#define RP_PLL_USB_BASE         0x4002C000u
#define RP_BUSCTRL_BASE         0x40030000u
#define RP_UART0_BASE           0x40034000u
#define RP_UART1_BASE           0x40038000u
#define RP_SPI0_BASE            0x4003C000u
#define RP_SPI1_BASE            0x40040000u
#define RP_I2C0_BASE            0x40044000u
#define RP_I2C1_BASE            0x40048000u
#define RP_ADC_BASE             0x4004C000u
#define RP_PWM_BASE             0x40050000u
#define RP_TIMER_BASE           0x40054000u
#define RP_WATCHDOG_BASE        0x40058000u
#define RP_RTC_BASE             0x4005C000u
#define RP_ROSC_BASE            0x40060000u
#define RP_VREG_CHIP_RESET_BASE 0x40064000u
#define RP_TBMAN_BASE           0x4006C000u

/* ------------------------------------------------------------------ */
/* Periféricos AHB-Lite                                                */
/* ------------------------------------------------------------------ */
#define RP_DMA_BASE             0x50000000u
#define RP_USB_DPRAM_BASE       0x50100000u   /* 4 KB RAM de doble puerto */
#define RP_USBCTRL_REGS_BASE    0x50110000u
#define RP_PIO0_BASE            0x50200000u
#define RP_PIO1_BASE            0x50300000u
#define RP_XIP_AUX_BASE         0x50400000u

/* ------------------------------------------------------------------ */
/* IOPORT y PPB                                                        */
/* ------------------------------------------------------------------ */
#define RP_SIO_BASE             0xD0000000u
#define RP_PPB_BASE             0xE0000000u
#define RP_SYSTICK_BASE         0xE000E010u
#define RP_NVIC_BASE            0xE000E100u

/* ------------------------------------------------------------------ */
/* Alias atómicos (§2.1.2)                                             */
/* Todo periférico APB/AHB tiene 4 vistas de sus registros:            */
/*   +0x0000 normal | +0x1000 XOR | +0x2000 SET | +0x3000 CLR          */
/* Escribir una máscara en la vista SET/CLR/XOR modifica solo esos     */
/* bits, sin lectura-modificación-escritura (no hay carreras con IRQ). */
/* NO aplica a SIO ni a PPB: SIO tiene sus propios registros _SET/_CLR.*/
/* ------------------------------------------------------------------ */
#define RP_ALIAS_XOR            0x1000u
#define RP_ALIAS_SET            0x2000u
#define RP_ALIAS_CLR            0x3000u

/* ------------------------------------------------------------------ */
/* Cabecera de la bootrom (§2.8.3)                                     */
/* Las tablas de la ROM usan punteros de 16 bits (la ROM está en       */
/* 0x0000-0x3FFF, así que caben).                                      */
/* ------------------------------------------------------------------ */
typedef struct {
    uint8_t  magia[3];        /* 0x10: 'M', 'u', 0x01                    */
    uint8_t  version;         /* 0x13: versión de la bootrom             */
    uint16_t tabla_funciones; /* 0x14: puntero a la tabla de funciones   */
    uint16_t tabla_datos;     /* 0x16: puntero a la tabla de datos       */
    uint16_t buscar_en_tabla; /* 0x18: puntero a rom_table_lookup()      */
} rp_rom_cabecera_t;

#define RP_ROM_CABECERA_DIR     0x00000010u

/*
 * GCC ≥ 12 trata las direcciones constantes menores que 4 KB como
 * "casi NULL" y avisa con -Warray-bounds al desreferenciarlas. La barrera
 * asm oculta el valor constante al optimizador; el acceso es legítimo.
 */
static inline const rp_rom_cabecera_t *rp_rom_cabecera(void)
{
    uintptr_t dir = RP_ROM_CABECERA_DIR;
    __asm__ volatile ("" : "+r"(dir));
    return (const rp_rom_cabecera_t *)dir;
}

/* Código de dos letras de una función de la ROM, p. ej. RP_ROM_CODIGO('P','3') */
#define RP_ROM_CODIGO(c1, c2)   ((uint32_t)(c1) | ((uint32_t)(c2) << 8))

/* Devuelve la dirección de la función de la ROM con ese código, o NULL. */
static inline void *rp_rom_funcion(char c1, char c2)
{
    typedef void *(*buscar_fn)(const uint16_t *tabla, uint32_t codigo);
    const rp_rom_cabecera_t *cab = rp_rom_cabecera();
    buscar_fn buscar = (buscar_fn)(uintptr_t)cab->buscar_en_tabla;
    return buscar((const uint16_t *)(uintptr_t)cab->tabla_funciones,
                  RP_ROM_CODIGO(c1, c2));
}

#endif /* RP2040_MAPA_H */
