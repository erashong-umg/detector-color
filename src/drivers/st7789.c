/*
 * st7789.c — Driver ST7789 sobre los registros del SPI0 (PL022).
 *
 * El módulo no tiene CS, así que una transacción es simplemente:
 *   DC = 0 → byte de comando → (esperar BSY = 0) → DC = 1 → datos.
 * DC solo puede cambiar cuando el PL022 terminó de sacar el último bit,
 * por eso cada cambio de DC va precedido de spi_esperar_fin().
 *
 * Los píxeles se envían con tramas de 16 bits (DSS = 16): el PL022 saca
 * primero el bit más significativo, que es justo el orden RGB565 big-endian
 * que espera el ST7789, y se hace una escritura a DR por píxel en vez de dos.
 */
#include "drivers/st7789.h"

#include <stdbool.h>
#include "configuracion/pantalla.h"
#include "configuracion/pines.h"
#include "hw/pin.h"
#include "hw/rp2040_regs.h"
#include "hw/tiempo.h"

#define TFT_SPI RP_SPI0

/* Comandos del ST7789 (datasheet ST7789V, §9.1) */
enum {
    CMD_SWRESET = 0x01,
    CMD_SLPOUT  = 0x11,
    CMD_NORON   = 0x13,
    CMD_INVOFF  = 0x20,
    CMD_INVON   = 0x21,
    CMD_DISPON  = 0x29,
    CMD_CASET   = 0x2A,
    CMD_RASET   = 0x2B,
    CMD_RAMWR   = 0x2C,
    CMD_MADCTL  = 0x36,
    CMD_COLMOD  = 0x3A
};

static uint32_t bits_actuales;

/* ------------------------------------------------------------------ */
/* Capa SPI (registros PL022)                                          */
/* ------------------------------------------------------------------ */

/* Espera a que salga el último bit y descarta lo recibido. El panel no
   tiene MISO conectado: lo que llega a la FIFO RX es basura y, como no se
   lee durante las ráfagas, se desborda; aquí se limpia ese desborde. */
static void spi_esperar_fin(void)
{
    while (!(TFT_SPI->sr & RP_SPI_SR_TFE)) { }
    while (TFT_SPI->sr & RP_SPI_SR_BSY) { }
    while (TFT_SPI->sr & RP_SPI_SR_RNE) {
        (void)TFT_SPI->dr;
    }
    TFT_SPI->icr = RP_SPI_ICR_RORIC;
}

/* El formato de trama solo se cambia con el periférico deshabilitado. */
static void spi_formato(uint32_t bits)
{
    if (bits == bits_actuales) {
        return;
    }
    spi_esperar_fin();
    TFT_SPI->cr1 = 0;
    TFT_SPI->cr0 = RP_SPI_CR0_DSS(bits)
                 | RP_SPI_CR0_SCR(TFT_SPI_SCR)
                 | (TFT_SPI_MODO3 ? (RP_SPI_CR0_SPO | RP_SPI_CR0_SPH) : 0u);
    TFT_SPI->cr1 = RP_SPI_CR1_SSE;
    bits_actuales = bits;
}

static inline void spi_escribir(uint32_t trama)
{
    while (!(TFT_SPI->sr & RP_SPI_SR_TNF)) { }
    TFT_SPI->dr = trama;
}

static void spi_iniciar(void)
{
    rp_liberar_reset(RP_RESET_SPI0);

    TFT_SPI->cr1  = 0;                 /* maestro, deshabilitado */
    TFT_SPI->cpsr = TFT_SPI_CPSDVSR;
    bits_actuales = 0;
    spi_formato(8);

    pin_funcion(PIN_TFT_SCK,  RP_FUNC_SPI);
    pin_funcion(PIN_TFT_MOSI, RP_FUNC_SPI);
}

/* ------------------------------------------------------------------ */
/* Protocolo ST7789                                                    */
/* ------------------------------------------------------------------ */

static void comando(uint8_t cmd, const uint8_t *datos, uint32_t n)
{
    spi_formato(8);
    pin_bajo(PIN_TFT_DC);
    spi_escribir(cmd);
    spi_esperar_fin();

    if (n > 0) {
        pin_alto(PIN_TFT_DC);
        for (uint32_t i = 0; i < n; i++) {
            spi_escribir(datos[i]);
        }
        spi_esperar_fin();
    }
}

/* Abre una ventana en la GRAM y deja el bus listo para recibir píxeles
   (RAMWR enviado, DC = 1, tramas de 16 bits). */
static void abrir_ventana(int x0, int y0, int x1, int y1)
{
    uint16_t cx0 = (uint16_t)(x0 + TFT_X_OFFSET), cx1 = (uint16_t)(x1 + TFT_X_OFFSET);
    uint16_t cy0 = (uint16_t)(y0 + TFT_Y_OFFSET), cy1 = (uint16_t)(y1 + TFT_Y_OFFSET);
    uint8_t columnas[4] = { cx0 >> 8, cx0 & 0xFF, cx1 >> 8, cx1 & 0xFF };
    uint8_t filas[4]    = { cy0 >> 8, cy0 & 0xFF, cy1 >> 8, cy1 & 0xFF };

    comando(CMD_CASET, columnas, 4);
    comando(CMD_RASET, filas, 4);
    comando(CMD_RAMWR, 0, 0);

    pin_alto(PIN_TFT_DC);
    spi_formato(16);
}

/* Recorta el rectángulo a la pantalla; devuelve false si queda vacío. */
static bool recortar(int *x, int *y, int *ancho, int *alto)
{
    if (*x < 0) { *ancho += *x; *x = 0; }
    if (*y < 0) { *alto  += *y; *y = 0; }
    if (*x + *ancho > TFT_ANCHO) { *ancho = TFT_ANCHO - *x; }
    if (*y + *alto  > TFT_ALTO)  { *alto  = TFT_ALTO  - *y; }
    return *ancho > 0 && *alto > 0;
}

/* ------------------------------------------------------------------ */
/* API pública                                                         */
/* ------------------------------------------------------------------ */

void st7789_iniciar(void)
{
    pin_salida(PIN_TFT_DC,  true);
    pin_salida(PIN_TFT_RST, true);
    spi_iniciar();

    /* Reset por hardware */
    tiempo_esperar_ms(5);
    pin_bajo(PIN_TFT_RST);
    tiempo_esperar_ms(20);
    pin_alto(PIN_TFT_RST);
    tiempo_esperar_ms(150);

    uint8_t v;
    comando(CMD_SWRESET, 0, 0);  tiempo_esperar_ms(150);
    comando(CMD_SLPOUT, 0, 0);   tiempo_esperar_ms(120);
    v = 0x55;       comando(CMD_COLMOD, &v, 1);        /* 16 bits/píxel */
    v = TFT_MADCTL; comando(CMD_MADCTL, &v, 1);
    comando(TFT_INVERTIR ? CMD_INVON : CMD_INVOFF, 0, 0);
    comando(CMD_NORON, 0, 0);    tiempo_esperar_ms(10);
    comando(CMD_DISPON, 0, 0);   tiempo_esperar_ms(50);

    st7789_rellenar(ST7789_NEGRO);
}

void st7789_rect(int x, int y, int ancho, int alto, uint16_t color)
{
    if (!recortar(&x, &y, &ancho, &alto)) {
        return;
    }
    abrir_ventana(x, y, x + ancho - 1, y + alto - 1);
    for (uint32_t n = (uint32_t)ancho * (uint32_t)alto; n > 0; n--) {
        spi_escribir(color);
    }
    spi_esperar_fin();
}

void st7789_rellenar(uint16_t color)
{
    st7789_rect(0, 0, TFT_ANCHO, TFT_ALTO, color);
}

void st7789_pixel(int x, int y, uint16_t color)
{
    st7789_rect(x, y, 1, 1, color);
}

void st7789_bloque(int x, int y, int ancho, int alto, const uint16_t *pixeles)
{
    if (x < 0 || y < 0 || ancho <= 0 || alto <= 0 ||
        x + ancho > TFT_ANCHO || y + alto > TFT_ALTO) {
        return;
    }
    abrir_ventana(x, y, x + ancho - 1, y + alto - 1);
    for (uint32_t n = (uint32_t)ancho * (uint32_t)alto; n > 0; n--) {
        spi_escribir(*pixeles++);
    }
    spi_esperar_fin();
}
