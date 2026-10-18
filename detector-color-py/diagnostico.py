"""Pruebas de banco desde el REPL (Ctrl+C para salir de cada una).

Sirven para medir las HIPÓTESIS de config.py antes de usar la aplicación completa.
No se importan desde main.py.
"""
import time

import config
from sensores import Botones, Finales, Joystick, Proximidad, Tcs3200, TEXTO_PROX


def _bucle(f, ms=200):
    try:
        while True:
            f()
            time.sleep_ms(ms)
    except KeyboardInterrupt:
        pass


def color_crudo():
    """Frecuencias del TCS3200 en Hz. Útil para ver el rango con negro y con blanco."""
    t = Tcs3200()
    _bucle(lambda: print("R,G,B =", t.medir()), 300)


def proximidad():
    """Nivel (0..4095, mayor = más cerca) y salida digital. Ajusta PROX_NIVEL_* y la polaridad."""
    p = Proximidad()
    def f():
        for _ in range(40):
            p.actualizar()
            time.sleep_ms(5)
        print("nivel", p.nivel, "OD", p.digital.value(), "->", TEXTO_PROX[p.estado])
    _bucle(f, 0)


def finales():
    """Estado de los dos finales de carrera: mueve el carrito a mano y comprueba FINAL_ACTIVO_BAJO."""
    f = Finales()
    def paso():
        f.actualizar()
        print("inicio", f.inicio, "(pin", f._pines[0].value(), ") fin", f.fin, "(pin", f._pines[1].value(), ")")
    _bucle(paso, 150)


def entradas():
    """Botones y joystick."""
    b, j = Botones(), Joystick()
    def paso():
        b.actualizar()
        print(sorted(b.eventos, key=str) or "", "joy", j.leer(), "centro", j.centro)
    _bucle(paso, 100)


def motor(vel=config.EJE_VEL_HOMING, segundos=1):
    """Pulso corto hacia el FIN y vuelta. Si el carrito va al revés, MOTOR_SIGNO = -1.
    CUIDADO: sin comprobar los finales; empieza con el carrito en el centro."""
    from actuadores import Motor
    m = Motor()
    m.habilitar(True)
    try:
        for v in (vel, 0, -vel, 0):
            m.velocidad(v)
            t0 = time.ticks_ms()
            while time.ticks_diff(time.ticks_ms(), t0) < segundos * 1000:
                m.actualizar()
                time.sleep_ms(1)
    finally:
        m.habilitar(False)


def rendimiento(segundos=10):
    """Mide cuánto tarda cada vuelta del bucle (sin motor). El motor y los finales se
    vigilan en cada vuelta, así que el MÁXIMO debe quedar por debajo de ~5 ms."""
    import gc
    from app import App
    a = App()
    gc.collect()
    n = peor = total = 0
    peor_dibujo = 0
    t_fin = time.ticks_add(time.ticks_ms(), segundos * 1000)
    while time.ticks_diff(t_fin, time.ticks_ms()) > 0:
        t0 = time.ticks_us()
        a.paso()
        dt = time.ticks_diff(time.ticks_us(), t0)
        n, total, peor = n + 1, total + dt, max(peor, dt)
        t0 = time.ticks_us()
        a._actualizar_modelo()
        a.ui.dibujar(a.modelo)
        peor_dibujo = max(peor_dibujo, time.ticks_diff(time.ticks_us(), t0))
        a._leer_entradas()
    print("paso(): media %d us, peor %d us (%d vueltas)" % (total // n, peor, n))
    print("modelo + dibujo: peor %d us" % peor_dibujo)
    gc.collect()
    print("memoria libre:", gc.mem_free(), "bytes")
