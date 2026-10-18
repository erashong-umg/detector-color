"""Sensores y entradas: TCS3200, IR de proximidad, finales de carrera, botones y joystick."""
import time
from machine import ADC, Pin, mem32

import config

ticks_ms, ticks_us, diff = time.ticks_ms, time.ticks_us, time.ticks_diff


# ======================================================================
# TCS3200: la frecuencia se cuenta por hardware con una slice PWM
# ======================================================================
# machine.PWM no sabe contar flancos de entrada, así que este es el único
# sitio que toca registros del RP2040 (PWM2, canal B = GP5).
_PWM = 0x40050000
_SLICE = (config.TCS_OUT >> 1) & 7
_CSR, _DIV, _CTR, _TOP = (_PWM + _SLICE * 0x14 + o for o in (0x00, 0x04, 0x08, 0x10))
_INTR = _PWM + 0xA4                       # marca de desborde del contador (W1C)
_IO_CTRL = 0x40014000 + 8 * config.TCS_OUT + 4

# filtros (S2, S3): rojo, verde, azul, claro
_FILTROS = ((0, 0), (1, 1), (0, 1), (1, 0))


class Tcs3200:
    def __init__(self):
        self.s2 = Pin(config.TCS_S2, Pin.OUT, value=0)
        self.s3 = Pin(config.TCS_S3, Pin.OUT, value=0)
        Pin(config.TCS_OUT, Pin.IN)       # configura el pad (sin pull)
        mem32[_IO_CTRL] = 4               # FUNCSEL = PWM
        mem32[_CSR] = 2 << 4              # DIVMODE = cuenta flancos de subida en B, EN = 0
        mem32[_DIV] = 1 << 4              # divisor 1.0
        mem32[_TOP] = 0xFFFF

    def _frecuencia(self, filtro):
        """Hz del canal, o None si el contador de 16 bits dio la vuelta."""
        self.s2.value(_FILTROS[filtro][0])
        self.s3.value(_FILTROS[filtro][1])
        time.sleep_us(config.TCS_ESPERA_FILTRO_US)
        mem32[_CTR] = 0
        mem32[_INTR] = 1 << _SLICE
        t0 = ticks_us()
        mem32[_CSR] = (2 << 4) | 1        # abre la ventana
        while diff(ticks_us(), t0) < config.TCS_VENTANA_US:
            pass
        mem32[_CSR] = 2 << 4              # cierra la ventana
        dt = diff(ticks_us(), t0)
        cuentas = mem32[_CTR] & 0xFFFF
        if mem32[_INTR] & (1 << _SLICE):
            return None
        return (cuentas * 1_000_000 + dt // 2) // dt

    def medir(self):
        """Promedio de MUESTRAS_COLOR lecturas -> (r, g, b) en Hz, o None si desbordó.
        Bloquea unos 90 ms por muestra: usar solo con el carrito parado."""
        suma = [0, 0, 0]
        for _ in range(config.MUESTRAS_COLOR):
            for i in range(3):
                f = self._frecuencia(i)
                if f is None:
                    return None
                suma[i] += f
        n = config.MUESTRAS_COLOR
        return suma[0] // n, suma[1] // n, suma[2] // n


# ======================================================================
# ADC (12 bits, como el RP2040: read_u16() trae 16 bits, se descartan 4)
# ======================================================================
def leer_adc(adc, n=1):
    return sum(adc.read_u16() >> 4 for _ in range(n)) // n


# ======================================================================
# IR de proximidad: ¿hay muestra y a qué distancia?
# ======================================================================
SIN_OBJETO, LEJOS, OK, CERCA = range(4)
TEXTO_PROX = ("nada", "lejos", "ok", "cerca")
_HISTERESIS = 100
_UMBRALES = (config.PROX_NIVEL_PRESENCIA, config.PROX_NIVEL_OK, config.PROX_NIVEL_CERCA)


class Proximidad:
    """Clasifica el nivel del IR con histéresis y exige 6 lecturas seguidas iguales."""

    def __init__(self):
        self.adc = ADC(Pin(config.IR_PROX_A))
        pull = Pin.PULL_UP if config.PROX_DIGITAL_ACTIVO_BAJO else Pin.PULL_DOWN
        self.digital = Pin(config.IR_PROX_D, Pin.IN, pull)
        self.estado = SIN_OBJETO
        self.nivel = 0
        self._candidato = SIN_OBJETO
        self._cuenta = 0
        self._t = ticks_ms()

    def actualizar(self):
        if diff(ticks_ms(), self._t) < 5:
            return
        self._t = ticks_ms()
        crudo = leer_adc(self.adc, 4)
        self.nivel = 4095 - crudo if config.PROX_ANALOGICO_INVERTIDO else crudo
        activo = (not self.digital.value()) == config.PROX_DIGITAL_ACTIVO_BAJO

        banda = self.estado
        while banda < 3 and self.nivel >= _UMBRALES[banda] + _HISTERESIS:
            banda += 1
        while banda > 0 and self.nivel < _UMBRALES[banda - 1] - _HISTERESIS:
            banda -= 1
        if activo and banda < LEJOS:
            banda = LEJOS

        if banda == self.estado:
            self._candidato, self._cuenta = banda, 0
        elif banda == self._candidato:
            self._cuenta += 1
            if self._cuenta >= 6:
                self.estado, self._cuenta = banda, 0
        else:
            self._candidato, self._cuenta = banda, 1


# ======================================================================
# Finales de carrera IR
# ======================================================================
class Finales:
    """`inicio` / `fin` = True cuando el carrito está en ese extremo (con antirrebote).
    El pull interno apunta hacia el estado activo: un cable suelto se lee como extremo
    alcanzado y el carrito no se mueve (fallo seguro)."""

    def __init__(self):
        pull = Pin.PULL_DOWN if config.FINAL_ACTIVO_BAJO else Pin.PULL_UP
        self._pines = (Pin(config.IR_INICIO, Pin.IN, pull), Pin(config.IR_FIN, Pin.IN, pull))
        self._estable = [False, False]
        self._cand = [False, False]
        self._t = [ticks_ms(), ticks_ms()]
        time.sleep_ms(config.FINAL_ANTIREBOTE_MS + 2)     # deja asentar el pull
        for i in (0, 1):
            self._estable[i] = self._cand[i] = self._leer(i)

    def _leer(self, i):
        return (not self._pines[i].value()) == config.FINAL_ACTIVO_BAJO

    def actualizar(self):
        ahora = ticks_ms()
        for i in (0, 1):
            lectura = self._leer(i)
            if lectura != self._cand[i]:
                self._cand[i], self._t[i] = lectura, ahora
            elif lectura != self._estable[i] and diff(ahora, self._t[i]) >= config.FINAL_ANTIREBOTE_MS:
                self._estable[i] = lectura

    @property
    def inicio(self):
        return self._estable[0]

    @property
    def fin(self):
        return self._estable[1]


# ======================================================================
# Botones B1..B4 y SW del joystick: eventos "pulsado", "largo" y "corto"
# ======================================================================
class Botones:
    """Nombres: 1..4 y 'sw'. Tras actualizar(), `eventos` es un conjunto con:
       ('pulsado', n), ('largo', n) y ('corto', n) (corto = soltado antes de la pulsación larga)."""

    def __init__(self):
        pines = dict(enumerate(config.BOTONES, 1))
        pines["sw"] = config.JOY_SW
        self._pines = {n: Pin(p, Pin.IN, Pin.PULL_UP) for n, p in pines.items()}
        ahora = ticks_ms()
        self._est = {n: not p.value() for n, p in self._pines.items()}
        self._cand = dict(self._est)
        self._t = {n: ahora for n in self._pines}
        self._largo = {n: False for n in self._pines}
        self.eventos = set()

    def actualizar(self):
        ahora = ticks_ms()
        self.eventos = ev = set()
        for n, pin in self._pines.items():
            lectura = not pin.value()                 # activo en bajo
            if lectura != self._cand[n]:
                self._cand[n], self._t[n] = lectura, ahora
            elif lectura != self._est[n] and diff(ahora, self._t[n]) >= config.BOTON_ANTIREBOTE_MS:
                self._est[n] = lectura
                if lectura:
                    ev.add(("pulsado", n))
                    self._largo[n] = False
                    self._t[n] = ahora
                elif not self._largo[n]:
                    ev.add(("corto", n))
            if self._est[n] and not self._largo[n] and diff(ahora, self._t[n]) >= config.BOTON_LARGO_MS:
                self._largo[n] = True
                ev.add(("largo", n))


# ======================================================================
# Joystick
# ======================================================================
def _normalizar(crudo, centro):
    """-100..100 con zona muerta, escalando cada mitad por separado."""
    if crudo >= centro:
        v = (crudo - centro) * 100 // max(1, 4095 - centro)
    else:
        v = -((centro - crudo) * 100 // max(1, centro))
    v = max(-100, min(100, v))
    zm = config.JOY_ZONA_MUERTA
    if abs(v) <= zm:
        return 0
    mag = (abs(v) - zm) * 100 // (100 - zm)
    return mag if v > 0 else -mag


class Joystick:
    def __init__(self):
        self._x = ADC(Pin(config.JOY_X))
        self._y = ADC(Pin(config.JOY_Y))
        # El centro se mide al arrancar: no tocar la palanca.
        self.centro = (leer_adc(self._x, 32), leer_adc(self._y, 32))

    def leer(self):
        """(x, y) en -100..100; y > 0 = arriba."""
        x = _normalizar(leer_adc(self._x, 4), self.centro[0])
        y = _normalizar(leer_adc(self._y, 4), self.centro[1])
        return x, (-y if config.JOY_INVERTIR_Y else y)
