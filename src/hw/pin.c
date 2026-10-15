/*
 * pin.c — Configuración de GPIO por registros.
 */
#include "pin.h"

void pin_inicializar(void)
{
    rp_liberar_reset(RP_RESET_IO_BANK0 | RP_RESET_PADS_BANK0);
}

void pin_funcion(uint32_t pin, rp_funcion_t funcion)
{
    /* Pad: entrada habilitada (IE=1) y salida no bloqueada (OD=0).
       Se usan los alias SET/CLR para tocar solo esos bits. */
    RP_PADS_BANK0_SET->gpio[pin] = RP_PAD_IE;
    RP_PADS_BANK0_CLR->gpio[pin] = RP_PAD_OD;

    /* Escribir CTRL entero pone los overrides a 0 (comportamiento normal). */
    RP_IO_BANK0->gpio[pin].ctrl = (uint32_t)funcion << RP_IO_CTRL_FUNCSEL_LSB;
}

void pin_pull(uint32_t pin, pin_pull_t pull)
{
    RP_PADS_BANK0_CLR->gpio[pin] = RP_PAD_PUE | RP_PAD_PDE;
    if (pull != PIN_SIN_PULL) {
        RP_PADS_BANK0_SET->gpio[pin] = (uint32_t)pull;
    }
}

void pin_salida(uint32_t pin, bool nivel_inicial)
{
    /* Fijar el nivel antes de habilitar el driver evita un pulso espurio. */
    pin_escribir(pin, nivel_inicial);
    RP_SIO->gpio_oe_set = 1u << pin;
    pin_funcion(pin, RP_FUNC_SIO);
}

void pin_entrada(uint32_t pin, pin_pull_t pull)
{
    RP_SIO->gpio_oe_clr = 1u << pin;
    pin_pull(pin, pull);
    pin_funcion(pin, RP_FUNC_SIO);
}

void pin_analogico(uint32_t pin)
{
    /* Sin buffer digital ni pulls: la tensión llega limpia al ADC. */
    RP_SIO->gpio_oe_clr = 1u << pin;
    RP_PADS_BANK0_CLR->gpio[pin] = RP_PAD_IE | RP_PAD_PUE | RP_PAD_PDE;
    RP_PADS_BANK0_SET->gpio[pin] = RP_PAD_OD;
    RP_IO_BANK0->gpio[pin].ctrl = RP_FUNC_NULL;
}
