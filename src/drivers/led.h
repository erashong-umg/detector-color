/*
 * led.h — LED RGB de ánodo común por PWM (R: slice 5 B, G: slice 6 A, B: slice 6 B).
 *
 * Los canales PWM están invertidos (el pin absorbe corriente), de modo que
 * 255 = brillo máximo y 0 = apagado, como en un LED de cátodo común.
 * Se aplica una corrección gamma 2,0 para que el brillo percibido sea
 * proporcional al valor, y la escala por canal de configuracion/actuadores.h.
 */
#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "dominio/color.h"

void led_iniciar(void);
void led_rgb(uint8_t r, uint8_t g, uint8_t b);
void led_color(color_rgb_t c);
void led_apagar(void);

#endif /* LED_H */
