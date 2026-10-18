"""Pruebas en el PC (CPython) del mismo código que corre en la Pico.

    python test_pc.py

Sustituye `machine`, `framebuf` y el reloj por simulaciones (una planta física para el
carrito) y comprueba: el cálculo de color, el homing y los viajes, y la aplicación
completa (calibrar, medir, mover, guardar, dibujar todas las pantallas).
No sirve para validar el cableado: eso solo se mide en el banco.
"""
import sys
import time
import types

# ---------------------------------------------------------------- reloj
T = [0]                                             # microsegundos simulados
time.ticks_us = lambda: T[0]
time.ticks_ms = lambda: T[0] // 1000
time.ticks_diff = lambda a, b: a - b
time.ticks_add = lambda a, b: a + b
time.sleep_us = lambda us: T.__setitem__(0, T[0] + us)
time.sleep_ms = lambda ms: T.__setitem__(0, T[0] + ms * 1000)

# ---------------------------------------------------------------- machine
NIVEL = {}                                          # pin -> nivel (entradas y salidas)


class Pin:
    IN, OUT, PULL_UP, PULL_DOWN = 0, 1, 2, 3

    def __init__(self, n, modo=None, pull=None, value=None):
        self.n = n
        if value is not None:
            NIVEL[n] = value
        NIVEL.setdefault(n, 1)

    def value(self, v=None):
        if v is None:
            return NIVEL[self.n]
        NIVEL[self.n] = v


class PWM:
    def __init__(self, pin, freq=0, duty_u16=0):
        self.pin, self._duty = pin.n, duty_u16

    def duty_u16(self, d=None):
        if d is None:
            return self._duty
        self._duty = d

    def freq(self, f):
        pass


ADC_VALOR = {}                                      # pin -> lectura de 12 bits


class ADC:
    def __init__(self, pin):
        self.n = pin.n

    def read_u16(self):
        return ADC_VALOR.get(self.n, 2048) << 4


class SPI:
    def __init__(self, *a, **k):
        self.bytes = 0

    def write(self, datos):
        self.bytes += len(datos)


class _Mem32(dict):
    def __missing__(self, k):
        return 0


machine = types.ModuleType("machine")
machine.Pin, machine.PWM, machine.ADC, machine.SPI, machine.mem32 = Pin, PWM, ADC, SPI, _Mem32()
sys.modules["machine"] = machine


class FrameBuffer:
    def __init__(self, buf, w, h, fmt, stride=None):
        self.w, self.h = w, h

    def fill_rect(self, *a): pass
    def pixel(self, *a): pass
    def blit(self, *a): pass


framebuf = types.ModuleType("framebuf")
framebuf.FrameBuffer, framebuf.RGB565, framebuf.MONO_HLSB = FrameBuffer, 1, 2
sys.modules["framebuf"] = framebuf

class StateMachine:
    """Cuenta pulsos a FRECUENCIA_SIM Hz mientras está activa (reloj simulado)."""
    FRECUENCIA_SIM = 50_000

    def __init__(self, *a, **k):
        self.x, self.t0 = 0, None

    def restart(self): pass

    def exec(self, instr):
        if instr.startswith("mov(x"):
            self.x = 0xFFFFFFFF

    def active(self, si):
        if si:
            self.t0 = T[0]
        elif self.t0 is not None:
            self.x -= self.FRECUENCIA_SIM * (T[0] - self.t0) // 1_000_000
            self.t0 = None

    def get(self):
        return self.x


rp2 = types.ModuleType("rp2")
rp2.asm_pio = lambda **k: (lambda f: f)
rp2.StateMachine = StateMachine
sys.modules["rp2"] = rp2

# ---------------------------------------------------------------- pruebas
import color                                        # noqa: E402
import config                                       # noqa: E402

fallos = comprobadas = 0


def comprobar(cond, texto):
    global fallos, comprobadas
    comprobadas += 1
    if not cond:
        fallos += 1
        print("   FALLO:", texto)


def titulo(t):
    print("\n==", t)


titulo("Color")
n, b = (1000, 1200, 900), (11000, 13000, 9500)
comprobar(color.normalizar(n, n, b) == (0, 0, 0), "negro")
comprobar(color.normalizar(b, n, b) == (255, 255, 255), "blanco")
comprobar(color.normalizar((6000, 1200, 900), n, b) == (128, 0, 0), "rojo a medias")
comprobar(color.normalizar(n, n, n) is None, "blanco <= negro")
comprobar(color.a_hsv((255, 0, 0)) == (0, 255, 255), "hsv rojo")
comprobar(color.a_hsv((0, 255, 0))[0] == 120 and color.a_hsv((0, 0, 255))[0] == 240, "hsv verde/azul")
comprobar(color.a_hsv((255, 0, 40))[0] == 351, "hsv rojo-magenta (división con signo): %s" % (color.a_hsv((255, 0, 40)),))
comprobar(color.a_hsv((100, 100, 100))[0] == -1, "gris sin tono")
comprobar(color.a_hex((255, 128, 0)) == "#FF8000", "hex")
comprobar(color.mapear((127, 200, 200))[1] is not None and abs(color.mapear((127, 200, 200))[1] - 112.9) < 0.2, "verde -> 112.9 mm")
comprobar(color.mapear((30, 10, 20))[0] == color.NEGRO and color.mapear((30, 10, 20))[1] is None, "negro no mueve")
comprobar(color.mapear((-1, 0, 100))[0] == color.GRIS and color.mapear((-1, 0, 220))[0] == color.BLANCO, "gris / blanco")
comprobar(color.mapear((358, 255, 255), 5.0)[1] == 0.0, "rojo 358 deg con el cursor al inicio va al inicio")
comprobar(abs(color.mapear((358, 255, 255), 300.0)[1] - 318.2) < 0.1, "rojo 358 deg con el cursor al final se queda en 318.2 mm")
comprobar(color.mapear((3, 255, 255), 300.0)[1] == config.ESCALA_ANCHO_MM, "rojo 3 deg con el cursor al final va al final")
comprobar(color.hay_que_mover(None, 10) and not color.hay_que_mover(10, 11) and color.hay_que_mover(10, 20), "zona muerta")

# ---------------------------------------------------------------- planta del carrito
import actuadores as act                            # noqa: E402
import carro as eje                                 # noqa: E402
from sensores import Finales                        # noqa: E402


class Planta:
    """Carrito ideal: velocidad (mm/s) = velocidad en % del motor. 40 % recorre 320 mm en 8 s."""

    def __init__(self, x, ruido=0.0):
        self.x, self.ruido, self.choque = x, ruido, False

    def paso(self, motor, dt_s):
        v = motor.real * config.MOTOR_SIGNO * (1 + self.ruido)
        self.x += v * dt_s
        if self.x < -1 or self.x > config.EJE_LONGITUD_MM + 1:
            self.choque = True
        self.x = max(-1.0, min(config.EJE_LONGITUD_MM + 1, self.x))
        # sensores activos en bajo: los finales ven el carrito a menos de 0,5 mm del tope
        NIVEL[config.IR_INICIO] = 0 if self.x <= 0.5 else 1
        NIVEL[config.IR_FIN] = 0 if self.x >= config.EJE_LONGITUD_MM - 0.5 else 1


def correr(planta, motor, ms, hasta=None, tick=lambda: None):
    for _ in range(ms):
        T[0] += 1000
        planta.paso(motor, 0.001)
        tick()
        if hasta and hasta():
            return True
    return hasta is None


titulo("Sensor de color (contador PIO simulado)")
from sensores import Tcs3200                        # noqa: E402
import sensores                                     # noqa: E402


def _reloj_que_avanza():                            # la espera activa de la ventana necesita que pase el tiempo
    T[0] += 50
    return T[0]


sensores.ticks_us = _reloj_que_avanza
sensor = Tcs3200()
f = sensor._frecuencia(0)
comprobar(f is not None and abs(f - 50_000) < 500, "frecuencia medida %s Hz (simulada 50000)" % f)
StateMachine.FRECUENCIA_SIM = 0
comprobar(sensor.medir() is None, "sin pulsos -> None")
StateMachine.FRECUENCIA_SIM = 50_000
comprobar(all(abs(v - 50_000) < 500 for v in sensor.medir()), "medir() promedia los 3 canales")
sensores.ticks_us = time.ticks_us

titulo("Carrito: homing, calibración y viajes")
for inicio, ruido in ((150.0, 0.0), (0.0, 0.0), (320.0, 0.0), (150.0, 0.03)):
    planta = Planta(inicio, ruido)
    planta.paso(act.Motor.__new__(act.Motor), 0) if False else None
    NIVEL[config.IR_INICIO] = 0 if inicio <= 0.5 else 1
    NIVEL[config.IR_FIN] = 0 if inicio >= 319.5 else 1
    motor = act.Motor()
    carro = eje.Carro(motor, Finales())
    comprobar(carro.armar(True), "armar")
    comprobar(carro.referenciar(), "referenciar")
    ok = correr(planta, motor, 40000, lambda: carro.estado == eje.LISTO, carro.actualizar)
    comprobar(ok and carro.calibrada, "homing desde %.0f mm (ruido %.0f%%): estado %s %s" % (
        inicio, ruido * 100, eje.TEXTO_ESTADO[carro.estado], carro.error))
    if not ok:
        continue
    print("   homing desde %5.1f mm: recorrido medido %d ms, cursor en %.1f mm" % (inicio, carro.recorrido_ms, planta.x))
    comprobar(planta.x < 2.0, "vuelve al inicio (%.1f mm)" % planta.x)
    comprobar(7000 < carro.recorrido_ms < 9500, "recorrido medido %d ms" % carro.recorrido_ms)
    errores = []
    for destino in (112.9, 250.0, 30.0, 160.0, 5.0, 315.0):
        comprobar(carro.ir_a(destino), "ir_a(%s)" % destino)
        correr(planta, motor, 20000, lambda: carro.estado == eje.LISTO, carro.actualizar)
        correr(planta, motor, 100, None, carro.actualizar)
        errores.append(abs(planta.x - destino))
    print("   error de posición en 6 destinos: máx %.1f mm" % max(errores))
    comprobar(max(errores) < 6.0, "error de posición %.1f mm" % max(errores))
    comprobar(not planta.choque, "sin choque contra los topes")
    comprobar(carro.estado == eje.LISTO and not carro.error, "termina LISTO sin error")

titulo("Carrito: fallos")
NIVEL[config.IR_INICIO], NIVEL[config.IR_FIN] = 0, 0
c = eje.Carro(act.Motor(), Finales())
comprobar(c.estado == eje.ERROR and not c.armar(True), "los dos finales activos al arrancar -> ERROR")
NIVEL[config.IR_INICIO], NIVEL[config.IR_FIN] = 1, 1
for _ in range(2):                                  # el antirrebote necesita que pase el tiempo
    T[0] += 20000
    c.finales.actualizar()
comprobar(c.reiniciar_error() and c.estado == eje.SIN_REF, "se recupera al soltar los finales")

# finales que nunca se activan: el timeout debe parar el motor
planta = Planta(150.0)
NIVEL[config.IR_INICIO] = NIVEL[config.IR_FIN] = 1
motor = act.Motor()
carro = eje.Carro(motor, Finales())
carro.armar(True)
carro.referenciar()
class Muerta:
    def paso(self, *a_):
        pass


correr(Muerta(), motor, 40000, lambda: carro.estado == eje.ERROR, carro.actualizar)
comprobar(carro.estado == eje.ERROR and "Tiempo" in carro.error and motor.real == 0, "timeout de homing: %s" % carro.error)

# sentido del motor invertido: busca el inicio y aparece el final contrario
config.MOTOR_SIGNO = -1
planta = Planta(150.0)
config.MOTOR_SIGNO = +1                             # el carro cree +1, la planta mueve al revés
NIVEL[config.IR_INICIO] = NIVEL[config.IR_FIN] = 1
motor = act.Motor()
carro = eje.Carro(motor, Finales())
carro.armar(True)
carro.referenciar()
for _ in range(40000):
    T[0] += 1000
    planta.x += -motor.real * 0.001
    planta.x = max(-1.0, min(321.0, planta.x))
    NIVEL[config.IR_INICIO] = 0 if planta.x <= 0.5 else 1
    NIVEL[config.IR_FIN] = 0 if planta.x >= 319.5 else 1
    carro.actualizar()
    if carro.estado == eje.ERROR:
        break
comprobar(carro.estado == eje.ERROR and "invertido" in carro.error, "sentido invertido detectado: %s" % carro.error)

# ---------------------------------------------------------------- aplicación completa
titulo("Aplicación: calibrar, medir, mover, guardar y dibujar")
import app as aplicacion                            # noqa: E402

NIVEL.clear()
planta = Planta(150.0)
NIVEL[config.IR_INICIO] = NIVEL[config.IR_FIN] = 1
for p in config.BOTONES + (config.JOY_SW,):
    NIVEL[p] = 1                                    # botones sin pulsar (pull-up)
ADC_VALOR[config.JOY_X] = ADC_VALOR[config.JOY_Y] = 2048
ADC_VALOR[config.IR_PROX_A] = 4095                  # nivel 0: sin objeto
NIVEL[config.IR_PROX_D] = 1
a = aplicacion.App()
frecuencia = [(1000, 1200, 900)]
a.tcs.medir = lambda: frecuencia[0]


def vuelta():
    a.paso()
    a.ui.dibujar(a.modelo)
    a._actualizar_modelo()


def esperar(cond, ms=30000):
    return correr(planta, a.motor, ms, cond, vuelta)


def poner_muestra(si):
    ADC_VALOR[config.IR_PROX_A] = 4095 - 2000 if si else 4095


comprobar(a.medir() is False and "calibrar" in a.modelo.mensaje, "medir sin calibrar se rechaza")
poner_muestra(True)
comprobar(a.calibrar(True), "pedir calibrar negro")
comprobar(esperar(lambda: a.negro is not None, 3000), "calibra el negro con la muestra a buena distancia")
frecuencia[0] = (11000, 13000, 9500)
comprobar(a.calibrar(False) and esperar(lambda: a.blanco is not None, 3000), "calibra el blanco")
poner_muestra(False)
comprobar(a.calibrar(True) and esperar(lambda: a.estado == aplicacion.IDLE, 12000) and "sin objeto" in a.modelo.mensaje.lower(),
          "sin muestra se cancela por tiempo (%s)" % a.modelo.mensaje)
comprobar(not a.referenciar(), "referenciar sin armar se rechaza")
comprobar(a.armar(True) and a.referenciar(), "armar y referenciar")
comprobar(esperar(lambda: a.estado == aplicacion.IDLE, 40000) and a.carro.calibrada, "homing termina")
poner_muestra(True)
frecuencia[0] = (1000 + int(10000 * 40 / 255), 1000 + int(12000 * 200 / 255), 900 + int(8600 * 60 / 255))   # verde
comprobar(a.medir(), "medir")
comprobar(esperar(lambda: a.estado == aplicacion.MOVIENDO, 3000), "tras medir mueve el cursor")
objetivo = a.x_lectura
comprobar(esperar(lambda: a.estado == aplicacion.IDLE, 30000), "llega al objetivo")
correr(planta, a.motor, 200, None, vuelta)
comprobar(a.lectura is not None and a.lectura[3] == color.CROMATICO and 105 < objetivo < 120,
          "lectura verde: %s -> %.1f mm" % (a.lectura and a.lectura[1], objetivo))
comprobar(abs(planta.x - objetivo) < 6.0, "cursor en %.1f mm, objetivo %.1f mm" % (planta.x, objetivo))
a.guardar()
comprobar(len(a.historial) == 1, "guardar en el historial")
frecuencia[0] = (1100, 1250, 950)                   # casi negro
comprobar(a.medir() and esperar(lambda: a.estado == aplicacion.IDLE, 3000) and "Sin tono" in a.modelo.mensaje and abs(planta.x - objetivo) < 6.0,
          "negro: el cursor no se mueve (%s)" % a.modelo.mensaje)
frecuencia[0] = None
comprobar(a.medir() and esperar(lambda: a.estado == aplicacion.IDLE, 3000) and "no válida" in a.modelo.mensaje, "desborde del sensor")
comprobar(a.ir_a(250.0) and esperar(lambda: a.estado == aplicacion.IDLE, 30000) and abs(planta.x - 250.0) < 6.0, "ir_a(250)")
a.ir_a(10.0)
correr(planta, a.motor, 300, None, vuelta)
a.cancelar()
comprobar(a.estado == aplicacion.IDLE and a.motor.real == 0, "cancelar un viaje frena el motor")

# navegación por todas las pantallas
e = aplicacion.ui.Entrada


def pulsar(**kw):
    x = e()
    for k, v in kw.items():
        setattr(x, k, v)
    orden = a.ui.procesar(x, a.modelo)
    a._ejecutar(orden)
    for _ in range(14):
        vuelta()
    return orden


pulsar(ok=True)                                     # menú
comprobar(a.ui.pantalla == aplicacion.ui.MENU, "SW abre el menú")
for _ in range(6):
    pulsar(dir=1)
pulsar(ok=True)                                     # historial
comprobar(a.ui.pantalla == aplicacion.ui.HISTORIAL, "menú -> historial")
pulsar(b4=True)
pulsar(b3=True)
comprobar(a.ui.pantalla == aplicacion.ui.MOVER, "B3 abre mover cursor")
pulsar(joy_x=100)
pulsar(ok=True)
comprobar(a.estado == aplicacion.MOVIENDO, "mover cursor ordena el viaje")
esperar(lambda: a.estado == aplicacion.IDLE, 30000)
pulsar(b2=True)
for _ in range(7):
    pulsar(dir=1)
pulsar(ok=True)
comprobar(a.ui.pantalla == aplicacion.ui.INFO, "menú -> información")
pulsar(b4=True)
comprobar(a.ui.pantalla == aplicacion.ui.PRINCIPAL, "B4 vuelve a la principal")

# error por los dos finales y recuperación con B4 largo
NIVEL[config.IR_INICIO] = NIVEL[config.IR_FIN] = 0
planta.paso = lambda *a_: None
correr(planta, a.motor, 50, None, vuelta)
comprobar(a.estado == aplicacion.ERROR and a.modelo.error, "error con los dos finales activos: %s" % a.modelo.error)
NIVEL[config.IR_INICIO] = NIVEL[config.IR_FIN] = 1
correr(planta, a.motor, 50, None, vuelta)
x = e()
x.b4_largo = True
a._entradas_de_estado(x)
comprobar(a.estado == aplicacion.IDLE, "B4 largo reinicia el error")

print("\n==========  %d comprobaciones, %d fallos  ==========" % (comprobadas, fallos))
sys.exit(1 if fallos else 0)
