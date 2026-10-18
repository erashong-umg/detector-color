"""Actuadores: motor (TB6612FNG), LED RGB de ánodo común y buzzer pasivo."""
import time
from machine import PWM, Pin

import config

ticks_ms, diff = time.ticks_ms, time.ticks_diff


class Motor:
    """Velocidad -100..100 con rampa (+ = hacia el final). Hay que llamar a actualizar()
    en cada vuelta del bucle."""

    def __init__(self):
        # Estado seguro antes de tocar el PWM: puente en standby, entradas a 0.
        self.stby = Pin(config.MOTOR_STBY, Pin.OUT, value=0)
        self.in1 = Pin(config.MOTOR_IN1, Pin.OUT, value=0)
        self.in2 = Pin(config.MOTOR_IN2, Pin.OUT, value=0)
        self.pwm = PWM(Pin(config.MOTOR_PWM), freq=20_000, duty_u16=0)   # fuera del rango audible
        self.habilitado = False
        self.objetivo = 0
        self.real = 0
        self._t = ticks_ms()

    def habilitar(self, si):
        if not si:
            self._aplicar(0)
            self.stby.value(0)
            self.objetivo = self.real = 0
        else:
            self.stby.value(1)
        self.habilitado = si

    def velocidad(self, v):
        self.objetivo = max(-100, min(100, v)) if self.habilitado else 0

    def frenar(self):
        """Freno inmediato por cortocircuito (IN1 = IN2 = 1)."""
        self.pwm.duty_u16(0)
        self.in1.value(1)
        self.in2.value(1)
        self.objetivo = self.real = 0

    def _aplicar(self, v):
        self.pwm.duty_u16(0)                     # nunca se cambian los IN con potencia aplicada
        self.in1.value(1 if v > 0 else 0)
        self.in2.value(1 if v < 0 else 0)
        if v:
            pct = config.MOTOR_DUTY_MIN_PCT + (100 - config.MOTOR_DUTY_MIN_PCT) * (abs(v) - 1) // 99
            self.pwm.duty_u16(pct * 65535 // 100)

    def actualizar(self):
        pasos = diff(ticks_ms(), self._t) // config.MOTOR_RAMPA_MS
        if not pasos:
            return
        self._t = time.ticks_add(self._t, pasos * config.MOTOR_RAMPA_MS)
        for _ in range(pasos):
            destino = self.objetivo
            if self.real * destino < 0:          # cambio de sentido: pasa por cero
                destino = 0
            if self.real == destino:
                break
            self.real += 1 if destino > self.real else -1
            self._aplicar(self.real)


class Led:
    """LED RGB de ánodo común: el PWM va invertido para que 255 = brillo máximo."""

    def __init__(self):
        self.canales = [PWM(Pin(p), freq=2000, duty_u16=65535)
                        for p in (config.LED_R, config.LED_G, config.LED_B)]

    def rgb(self, r, g, b):
        for pwm, v in zip(self.canales, (r, g, b)):
            gamma = v * v // 255                 # gamma 2,0 para que el brillo parezca lineal
            pwm.duty_u16(65535 - gamma * 257)

    def apagar(self):
        self.rgb(0, 0, 0)


# Sonidos: lista de (frecuencia Hz, ms); 0 Hz = pausa
INICIO = ((523, 90), (659, 90), (784, 90), (1047, 180))
CLIC = ((2000, 15),)
CONFIRMAR = ((1047, 80), (1319, 80), (1568, 160))
ALERTA = ((880, 120), (0, 80), (880, 120), (0, 80), (880, 120))
ERROR = ((400, 200), (0, 40), (300, 350))


class Buzzer:
    """Reproduce una lista de notas sin bloquear: llamar a actualizar() en cada vuelta."""

    def __init__(self):
        self.pwm = PWM(Pin(config.BUZZER), freq=1000, duty_u16=0)
        self._notas = ()
        self._i = 0
        self._t = 0

    def _nota(self):
        hz, _ = self._notas[self._i]
        if hz:
            self.pwm.freq(hz)
            self.pwm.duty_u16(65535 * 40 // 100)     # 40 % = volumen moderado
        else:
            self.pwm.duty_u16(0)
        self._t = ticks_ms()

    def reproducir(self, notas):
        self._notas, self._i = notas, 0
        self._nota()

    def silenciar(self):
        self._notas = ()
        self.pwm.duty_u16(0)

    def actualizar(self):
        if self._notas and diff(ticks_ms(), self._t) >= self._notas[self._i][1]:
            self._i += 1
            if self._i < len(self._notas):
                self._nota()
            else:
                self.silenciar()
