"""Aplicación: une sensores, carrito, pantalla y realimentación (LED y buzzer).

Estados: IDLE, HOMING, ESPERA (esperando la muestra para medir o calibrar),
MOVIENDO, MANUAL y ERROR.

Controles:
  Principal: B1 medir · B2 menú de calibración · B3 mover cursor ·
             B4 corto guarda la lectura · B4 largo o SW abre el menú
  Operación en curso: B4 o SW la cancelan
  MANUAL: joystick X mueve el cursor · B4 o SW salen
  ERROR: B4 largo reinicia el error
"""
import gc
import time

import actuadores as act
import carro as eje
import color
import config
import ui
from pantalla import Pantalla, rgb565
from sensores import Botones, Finales, Joystick, Proximidad, Tcs3200, OK, TEXTO_PROX

ticks_ms, diff = time.ticks_ms, time.ticks_diff

IDLE, HOMING, ESPERA, MOVIENDO, MANUAL, ERROR = range(6)
HISTORIAL_MAX = 16
JOY_UMBRAL_DIR = 55
MENSAJE_MS = 3000


class App:
    def __init__(self):
        self.pantalla = Pantalla()                  # primero: SPI0 reserva GP4 (ver pantalla.py)
        self.botones = Botones()
        self.joystick = Joystick()
        self.buzzer = act.Buzzer()
        self.led = act.Led()
        self.tcs = Tcs3200()
        self.prox = Proximidad()
        self.motor = act.Motor()
        self.finales = Finales()
        self.carro = eje.Carro(self.motor, self.finales)
        self.ui = ui.Interfaz(self.pantalla)
        self.modelo = ui.Modelo()

        self.estado = IDLE
        self.accion = None                          # "medir", "cal_negro" o "cal_blanco" en ESPERA
        self.negro = self.blanco = None             # calibración de color: (r, g, b) en Hz
        self.lectura = None                         # (rgb, hex, hsv, clase)
        self.x_lectura = None                       # posición de la lectura (mm) o None
        self.historial = []
        self._t_msg = 0
        self._guia = None
        self._t_espera = self._t_ok = 0
        self._ok_activo = False
        self._b4_largo_visto = False
        self._dir_ant = 0
        self._t_dir = 0
        if self.carro.estado == eje.ERROR:
            self._entrar_error()
        else:
            self.buzzer.reproducir(act.INICIO)
            self._mensaje("Calibra negro y blanco para empezar")

    # ---- mensajes y realimentación ------------------------------------
    def _mensaje(self, texto):
        self.modelo.mensaje = texto
        self._t_msg = ticks_ms()

    def _rechazar(self, texto):
        self._mensaje(texto)
        self.buzzer.reproducir(act.ALERTA)
        return False

    def _volver_a_idle(self):
        self.estado, self.accion, self._guia, self._ok_activo = IDLE, None, None, False

    def _entrar_error(self):
        self.estado, self.accion = ERROR, None
        self.carro.manual(0)
        self.buzzer.reproducir(act.ERROR)
        self.led.rgb(255, 0, 0)

    # ---- órdenes -------------------------------------------------------
    def _exigir_idle(self):
        if self.estado == ERROR:
            return self._rechazar("Hay un error: B4 largo lo reinicia")
        if self.estado != IDLE:
            return self._rechazar("Espera: hay una operación en curso")
        return True

    def armar(self, si):
        if not self.carro.armar(si):
            return self._rechazar("Hay un error: B4 largo lo reinicia")
        if not si and self.estado in (HOMING, MOVIENDO, MANUAL):
            self._volver_a_idle()
        self._mensaje("Motor armado" if si else "Motor desarmado")
        self.buzzer.reproducir(act.CONFIRMAR if si else act.CLIC)
        return True

    def referenciar(self):
        if not self._exigir_idle():
            return False
        if not self.carro.armado:
            return self._rechazar("Arma el motor primero")
        if not self.carro.referenciar():
            return self._rechazar("Hay un error: B4 largo lo reinicia")
        self.estado = HOMING
        self._mensaje("Referenciando el eje...")
        return True

    def _esperar_muestra(self, accion):
        if not self._exigir_idle():
            return False
        self.estado, self.accion = ESPERA, accion
        self._t_espera, self._ok_activo, self._guia = ticks_ms(), False, None
        return True

    def medir(self):
        if self.negro is None or self.blanco is None:
            return self._rechazar("Falta calibrar negro y blanco") if self._exigir_idle() else False
        return self._esperar_muestra("medir")

    def calibrar(self, negro):
        return self._esperar_muestra("cal_negro" if negro else "cal_blanco")

    def ir_a(self, mm):
        if self.estado == ERROR:
            return self._rechazar("Hay un error: B4 largo lo reinicia")
        if self.estado not in (IDLE, MOVIENDO):
            return self._rechazar("Espera: hay una operación en curso")
        if not self.carro.armado:
            return self._rechazar("Arma el motor primero")
        if not self.carro.calibrada:
            return self._rechazar("Referencia el eje primero")
        self.carro.ir_a(mm)
        self.x_lectura = mm
        self.estado = MOVIENDO
        self._mensaje("Moviendo el cursor")
        return True

    def manual(self):
        if not self._exigir_idle():
            return False
        if not self.carro.armado:
            return self._rechazar("Arma el motor primero")
        self.estado = MANUAL
        self._mensaje("Manual: joystick X · B4 sale")
        return True

    def guardar(self):
        if self.lectura is None:
            self._mensaje("Nada que guardar: mide primero")
            self.buzzer.reproducir(act.ALERTA)
            return
        if len(self.historial) == HISTORIAL_MAX:
            self.historial.pop(0)
        self.historial.append((self.lectura[0], self.lectura[2][0], self.x_lectura))
        self._mensaje("Lectura guardada (%d)" % len(self.historial))
        self.buzzer.reproducir(act.CLIC)

    def cancelar(self):
        if self.estado in (HOMING, MOVIENDO, MANUAL, ESPERA):
            self.carro.parar()
            self._volver_a_idle()
            self.led.apagar()
            self.buzzer.reproducir(act.CLIC)
            self._mensaje("Cancelado")

    def _ejecutar(self, orden):
        if orden is None:
            return
        if isinstance(orden, tuple):
            self.ir_a(orden[1])
            return
        {"medir": self.medir,
         "cal_negro": lambda: self.calibrar(True),
         "cal_blanco": lambda: self.calibrar(False),
         "referenciar": self.referenciar,
         "guardar": self.guardar,
         "armar": lambda: self.armar(True),
         "desarmar": lambda: self.armar(False),
         "manual": self.manual}[orden]()

    # ---- esperar la muestra y medir -----------------------------------
    def _paso_espera(self):
        p = self.prox.estado
        guia = ("No hay objeto: colócalo en el sensor", "Acerca el objeto al sensor",
                None, "Aleja un poco el objeto")[p]
        if guia != self._guia:
            self._guia = guia
            self.buzzer.reproducir(act.ALERTA if guia else act.CLIC)
            self.led.rgb(255, 160, 0) if guia else self.led.rgb(0, 200, 90)
        self._mensaje(guia or "Objeto a buena distancia: no lo muevas")

        ahora = ticks_ms()
        if p == OK:
            if not self._ok_activo:
                self._ok_activo, self._t_ok = True, ahora
            listo = diff(ahora, self._t_ok) >= config.PROX_ESTABLE_MS
        else:
            self._ok_activo, listo = False, False

        if listo and self.carro.quieto():
            self._ejecutar_accion()
        elif diff(ahora, self._t_espera) >= config.ESPERA_OBJETO_MS:
            self._volver_a_idle()
            self.led.apagar()
            self.buzzer.reproducir(act.ERROR)
            self._mensaje("Sin objeto: medición cancelada")

    def _ejecutar_accion(self):
        accion = self.accion
        self._volver_a_idle()
        frec = self.tcs.medir()                      # bloquea ~270 ms: el carrito está parado
        if frec is None:
            self.buzzer.reproducir(act.ERROR)
            self._mensaje("Lectura no válida (desborde)")
            return
        if accion == "cal_negro":
            self.negro = frec
            self._mensaje("Negro calibrado")
            self.buzzer.reproducir(act.CONFIRMAR)
            return
        if accion == "cal_blanco":
            self.blanco = frec
            self._mensaje("Blanco calibrado")
            self.buzzer.reproducir(act.CONFIRMAR)
            return

        rgb = color.normalizar(frec, self.negro, self.blanco)
        if rgb is None:
            self.buzzer.reproducir(act.ERROR)
            self._mensaje("Lectura no válida (revisa la calibración)")
            return
        hsv = color.a_hsv(rgb)
        clase, x = color.mapear(hsv, self.carro.posicion_mm)
        self.lectura = (rgb, color.a_hex(rgb), hsv, clase)
        self.x_lectura = x
        self.led.rgb(*rgb)
        if x is None:
            self.buzzer.reproducir(act.ALERTA)
            self._mensaje("Sin tono: el cursor no se mueve")
        elif not self.carro.calibrada:
            self.buzzer.reproducir(act.CONFIRMAR)
            self._mensaje("Medido (eje sin referenciar)")
        elif not color.hay_que_mover(self.carro.posicion_mm, x):
            self.buzzer.reproducir(act.CONFIRMAR)
            self._mensaje("El cursor ya apunta aquí")
        elif self.carro.armado and self.carro.ir_a(x):
            self.estado = MOVIENDO
            self.buzzer.reproducir(act.CONFIRMAR)
            self._mensaje("Moviendo el cursor")
        else:
            self.buzzer.reproducir(act.CONFIRMAR)
            self._mensaje("Medido (motor desarmado)")

    # ---- entradas ------------------------------------------------------
    def _leer_entradas(self):
        self.botones.actualizar()
        ev = self.botones.eventos
        e = ui.Entrada()
        e.b1, e.b2, e.b3 = ("pulsado", 1) in ev, ("pulsado", 2) in ev, ("pulsado", 3) in ev
        e.ok = ("pulsado", "sw") in ev
        e.b4 = ("corto", 4) in ev
        e.b4_largo = ("largo", 4) in ev
        x, y = self.joystick.leer()
        e.joy_x = x
        # Un pulso arriba/abajo por movimiento de palanca, con repetición.
        d = -1 if y > JOY_UMBRAL_DIR else (1 if y < -JOY_UMBRAL_DIR else 0)
        ahora = ticks_ms()
        if d and (d != self._dir_ant or diff(ahora, self._t_dir) >= 220):
            e.dir, self._t_dir = d, ahora
        self._dir_ant = d
        return e

    def _entradas_de_estado(self, e):
        """Cancelar, salir del modo manual o reiniciar un error. Consume lo que usa."""
        if self.estado in (HOMING, ESPERA, MOVIENDO):
            if e.b4 or e.ok:
                self.cancelar()
                e.b4 = e.ok = False
        elif self.estado == MANUAL:
            self.carro.manual(e.joy_x * config.EJE_VEL_VIAJE // 100)
            if e.b4 or e.ok:
                self.carro.manual(0)
                self._volver_a_idle()
                self._mensaje("Modo manual terminado")
                e.b4 = e.ok = False
        elif self.estado == ERROR and e.b4_largo:
            if self.carro.reiniciar_error():
                self._volver_a_idle()
                self.led.apagar()
                self.buzzer.reproducir(act.CONFIRMAR)
                self._mensaje("Error reiniciado: referencia el eje")
            else:
                self._mensaje("El error persiste: revisa los finales")
                self.buzzer.reproducir(act.ERROR)
            e.b4_largo = False

    # ---- modelo para la interfaz --------------------------------------
    def _actualizar_modelo(self):
        m, c = self.modelo, self.carro
        m.armado, m.ref, m.cal = c.armado, c.calibrada, self.negro is not None and self.blanco is not None
        m.hay_negro, m.hay_blanco = self.negro is not None, self.blanco is not None
        if self.estado == ERROR:
            m.estado, m.estado_color = "ERROR", ui.C_ERROR
        elif self.estado == HOMING:
            m.estado, m.estado_color = ("CALIBRANDO" if c.estado == eje.CALIBRANDO else "HOMING"), ui.C_AVISO
        elif self.estado == ESPERA:
            m.estado, m.estado_color = ("MIDIENDO" if self.accion == "medir" else "CAL. COLOR"), ui.C_AVISO
        elif self.estado == MOVIENDO:
            m.estado, m.estado_color = "MOVIENDO", ui.C_ACENTO
        elif self.estado == MANUAL:
            m.estado, m.estado_color = "MANUAL", ui.C_ACENTO
        elif not m.armado:
            m.estado, m.estado_color = "DESARMADO", ui.C_TENUE
        elif not m.ref:
            m.estado, m.estado_color = "SIN REF.", ui.C_AVISO
        else:
            m.estado, m.estado_color = "LISTO", ui.C_OK
        m.pos_mm = c.posicion_mm
        m.eje = eje.TEXTO_ESTADO[c.estado]
        m.ir_inicio, m.ir_fin = self.finales.inicio, self.finales.fin
        m.prox = TEXTO_PROX[self.prox.estado]
        m.recorrido_ms = c.recorrido_ms if c.calibrada else 0
        m.error = c.error if self.estado == ERROR else ""
        m.lectura = self.lectura
        m.objetivo_mm = self.x_lectura
        m.historial = self.historial
        if m.mensaje and self.estado != ESPERA and diff(ticks_ms(), self._t_msg) >= MENSAJE_MS:
            m.mensaje = ""

    # ---- bucle ---------------------------------------------------------
    def paso(self):
        """Una vuelta del bucle principal (llamar lo más seguido posible)."""
        self.buzzer.actualizar()
        self.prox.actualizar()
        self.carro.actualizar()                      # finales, rampa, tiempos: en cada vuelta

        if self.carro.estado == eje.ERROR and self.estado != ERROR:
            self._entrar_error()
        elif self.estado == HOMING and self.carro.estado == eje.LISTO and self.carro.calibrada:
            self._volver_a_idle()
            self.buzzer.reproducir(act.CONFIRMAR)
            self._mensaje("Eje referenciado")
        elif self.estado == ESPERA:
            self._paso_espera()
        elif self.estado == MOVIENDO and self.carro.estado == eje.LISTO:
            self._volver_a_idle()
            self.buzzer.reproducir(act.CONFIRMAR)
            self._mensaje("Cursor en posición")

    def ejecutar(self):
        t_entrada = t_dibujo = t_gc = ticks_ms()
        while True:
            self.paso()
            ahora = ticks_ms()
            # Recolectar basura con frecuencia mantiene cada pausa corta (<~5 ms); con el
            # montón lleno una sola pausa puede pasar de 20 ms y frenar el motor a destiempo.
            if diff(ahora, t_gc) >= 500 and self.carro.motor.real == 0:
                t_gc = ahora
                gc.collect()
            if diff(ahora, t_entrada) >= 10:         # el joystick son 8 conversiones del ADC
                t_entrada = ahora
                e = self._leer_entradas()
                self._entradas_de_estado(e)
                self._actualizar_modelo()
                self._ejecutar(self.ui.procesar(e, self.modelo))
                self._actualizar_modelo()
            if diff(ahora, t_dibujo) >= 8:
                t_dibujo = ahora
                self.ui.dibujar(self.modelo)         # una sección por llamada
