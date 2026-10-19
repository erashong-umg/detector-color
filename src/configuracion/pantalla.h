/*
 * pantalla.h — Parámetros del módulo ST7789 de 2" (240×320, CS fijo a GND, BL a 3V3).
 *
 * Valores heredados de Lunar Lander (módulo sin CS, 240×240); con este módulo
 * (CS a GND), sin verificar. Diagnóstico rápido:
 *   pantalla negra        → revisar SCK/SDA o probar TFT_SPI_MODO3 0
 *   colores invertidos    → TFT_INVERTIR 0
 *   imagen desplazada     → TFT_X_OFFSET / TFT_Y_OFFSET a 80
 *   rayas o ruido         → TFT_SPI_SCR 1 (baja el reloj a la mitad)
 */
#ifndef PANTALLA_H
#define PANTALLA_H

#define TFT_ANCHO        240
#define TFT_ALTO         320     /* con 240 se veía ruido debajo: la RAM del ST7789 es de 240x320 */

#define TFT_SPI_MODO3    1      /* sin CS el ST7789 necesita CPOL=1, CPHA=1 */
#define TFT_INVERTIR     1      /* panel IPS: INVON                         */
#define TFT_MADCTL       0x00   /* orientación (0x00, 0x60, 0xC0, 0xA0)     */
#define TFT_X_OFFSET     0
#define TFT_Y_OFFSET     0

/* Reloj SPI = clk_peri / (CPSDVSR × (1 + SCR)).
   Con clk_peri = 125 MHz (configurado por el runtime del SDK):
   CPSDVSR = 2, SCR = 0 → 62,5 MHz (el máximo del PL022 como maestro). */
#define TFT_SPI_CPSDVSR  2      /* par, 2..254 */
#define TFT_SPI_SCR      0      /* 0..255      */

#endif /* PANTALLA_H */
