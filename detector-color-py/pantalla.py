"""Pantalla ST7789 240x240 (sin CS) por SPI0: rectángulos y texto con fuente 5x7.

Se dibuja directo en la memoria del controlador (no hay framebuffer completo en la
Pico). Los colores son RGB565 (ver rgb565()).

Ojo: SPI(0) de MicroPython reserva también el pin MISO por defecto (GP4 = IR de inicio).
Crear esta pantalla ANTES que los sensores; así Finales() deja GP4 como entrada.
"""
import framebuf
import time
from machine import Pin, SPI

import config
import fuente

ANCHO, ALTO = config.TFT_ANCHO, config.TFT_ALTO


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


NEGRO, BLANCO = 0x0000, 0xFFFF


def _sw(c):
    """El framebuf guarda RGB565 little-endian y el panel lo quiere big-endian."""
    return ((c & 0xFF) << 8) | (c >> 8)


class Pantalla:
    def __init__(self):
        self.dc = Pin(config.TFT_DC, Pin.OUT, value=1)
        self.rst = Pin(config.TFT_RST, Pin.OUT, value=1)
        self.spi = SPI(0, baudrate=config.TFT_BAUDIOS, polarity=1, phase=1,   # sin CS: modo 3
                       sck=Pin(config.TFT_SCK), mosi=Pin(config.TFT_MOSI))
        self._linea = bytearray(ANCHO * 4 * 2 * 8)            # texto de hasta escala 4
        self._glifos = {}
        self._paleta = framebuf.FrameBuffer(bytearray(4), 2, 1, framebuf.RGB565)

        # Reset por hardware y secuencia de arranque
        time.sleep_ms(5)
        self.rst.value(0)
        time.sleep_ms(20)
        self.rst.value(1)
        time.sleep_ms(150)
        self._cmd(0x01)                                       # SWRESET
        time.sleep_ms(150)
        self._cmd(0x11)                                       # SLPOUT
        time.sleep_ms(120)
        self._cmd(0x3A, b"\x55")                              # COLMOD: 16 bits/píxel
        self._cmd(0x36, bytes((config.TFT_MADCTL,)))          # MADCTL: orientación
        self._cmd(0x21 if config.TFT_INVERTIR else 0x20)      # INVON / INVOFF
        self._cmd(0x13)                                       # NORON
        time.sleep_ms(10)
        self._cmd(0x29)                                       # DISPON
        time.sleep_ms(50)
        self.rellenar(NEGRO)

    # ---- bajo nivel ----------------------------------------------------
    def _cmd(self, c, datos=None):
        self.dc.value(0)
        self.spi.write(bytes((c,)))
        if datos:
            self.dc.value(1)
            self.spi.write(datos)

    def _ventana(self, x0, y0, x1, y1):
        self._cmd(0x2A, bytes((x0 >> 8, x0 & 255, x1 >> 8, x1 & 255)))   # CASET
        self._cmd(0x2B, bytes((y0 >> 8, y0 & 255, y1 >> 8, y1 & 255)))   # RASET
        self._cmd(0x2C)                                                    # RAMWR
        self.dc.value(1)

    # ---- primitivas ----------------------------------------------------
    def rect(self, x, y, w, h, color):
        if x < 0:
            w, x = w + x, 0
        if y < 0:
            h, y = h + y, 0
        w, h = min(w, ANCHO - x), min(h, ALTO - y)
        if w <= 0 or h <= 0:
            return
        self._ventana(x, y, x + w - 1, y + h - 1)
        fila = bytes((color >> 8, color & 255)) * w
        k = max(1, 1024 // len(fila))                         # filas por escritura
        bloque = fila * k
        while h >= k:
            self.spi.write(bloque)
            h -= k
        if h:
            self.spi.write(fila * h)

    def rellenar(self, color):
        self.rect(0, 0, ANCHO, ALTO, color)

    def bloque(self, x, y, w, h, datos):
        """Copia píxeles RGB565 big-endian (w*h*2 bytes) a la pantalla."""
        if x < 0 or y < 0 or x + w > ANCHO or y + h > ALTO:
            return
        self._ventana(x, y, x + w - 1, y + h - 1)
        self.spi.write(datos)

    # ---- texto ---------------------------------------------------------
    def _glifo(self, ch, esc):
        """Máscara monocroma del carácter a la escala dada (se guarda en una caché)."""
        clave = (ch, esc)
        g = self._glifos.get(clave)
        if g is None:
            ancho = (6 * esc + 7) & ~7                        # MONO_HLSB pide filas de 8 en 8
            alto = 8 * esc
            buf = bytearray(ancho // 8 * alto)
            g = framebuf.FrameBuffer(buf, ancho, alto, framebuf.MONO_HLSB)
            cols = fuente.glifo(ord(ch))
            for c in range(5):
                for f in range(7):
                    if cols[c] & (1 << f):
                        g.fill_rect(c * esc, f * esc, esc, esc, 1)
            self._glifos[clave] = g = (g, buf)
        return g[0]

    def ancho_texto(self, s, esc):
        return len(s) * 6 * esc

    def texto(self, x, y, s, esc, fg, bg, campo=0):
        """Dibuja `s` con la esquina superior izquierda en (x, y); si tiene menos de
        `campo` caracteres se rellena con espacios (así se borra el texto anterior).
        Devuelve el ancho dibujado en píxeles."""
        s = s + " " * (campo - len(s))
        if y < 0 or y + 8 * esc > ALTO or x >= ANCHO:
            return 0
        s = s[:(ANCHO - x) // (6 * esc)]                      # no recorta: deja de dibujar
        if not s:
            return 0
        w, h = len(s) * 6 * esc, 8 * esc
        buf = memoryview(self._linea)[:w * h * 2]
        fb = framebuf.FrameBuffer(buf, w, h, framebuf.RGB565)
        self._paleta.pixel(0, 0, _sw(bg))
        self._paleta.pixel(1, 0, _sw(fg))
        for i, ch in enumerate(s):
            fb.blit(self._glifo(ch, esc), i * 6 * esc, 0, -1, self._paleta)
        self.bloque(x, y, w, h, buf)
        return w

    def texto_centrado(self, x, ancho, y, s, esc, fg, bg):
        return self.texto(x + (ancho - self.ancho_texto(s, esc)) // 2, y, s, esc, fg, bg)
