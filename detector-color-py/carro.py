"""Carrito sobre el riel: seguridad, homing y posición por tiempo (sin encoder).

La posición se estima integrando la velocidad aplicada (% · µs). Al calibrar, el
acumulado entre INICIO y FIN fija la escala; el final de carrera de INICIO pone el
acumulado a 0 y el de FIN a su total, así que el error no se acumula entre viajes.

Convención: velocidad > 0 = hacia el FIN, < 0 = hacia el INICIO.
actualizar() debe llamarse en cada vuelta del bucle (lo ideal es <= 2 ms).
"""
import time

import config

ticks_us, ticks_ms, diff = time.ticks_us, time.ticks_ms, time.ticks_diff

SIN_REF, BUSCANDO, CALIBRANDO, MOVIENDO, LISTO, ERROR = range(6)
TEXTO_ESTADO = ("SIN_REFERENCIA", "BUSCANDO_INICIO", "CALIBRANDO", "MOVIENDO", "LISTO", "ERROR")

_A_INICIO, _A_FIN, _A_POS = range(3)


class Carro:
    def __init__(self, motor, finales):
        self.motor = motor
        self.finales = finales
        self.estado = SIN_REF
        self.error = ""                  # texto del error activo ("" = ninguno)
        self.calibrada = False
        self.recorrido_ms = 0            # duración medida de la calibración
        self._acum = 0                   # % · µs desde el inicio
        self._total = 0                  # _acum medido entre inicio y fin
        self._pedida = 0
        self._manual = 0
        self._destino = 0.0
        self._tipo = _A_POS
        self._fin_previo = False
        self._t_ult = ticks_us()
        self._t_cal = 0
        self._mov = False                # hay movimiento continuo en curso
        self._t_mov = 0
        self._timeout = config.CARRO_TIMEOUT_MS
        self.finales.actualizar()
        if self.finales.inicio and self.finales.fin:
            self._fallar("Los dos finales activos a la vez")

    # ---- órdenes -------------------------------------------------------
    @property
    def armado(self):
        return self.motor.habilitado

    def armar(self, si):
        if si and self.estado == ERROR:
            return False
        if not si:
            self.parar()
        self.motor.habilitar(si)
        return True

    def referenciar(self):
        """Homing + calibración + vuelta al inicio."""
        if not self.armado or self.estado == ERROR:
            return False
        self.calibrada = False
        self._manual = 0
        self._fin_previo = self.finales.fin
        self._timeout = config.EJE_RECORRIDO_ESPERADO_MS * 2
        self.estado = BUSCANDO
        return True

    def ir_a(self, mm):
        """Va a `mm` (0..longitud). Los extremos se alcanzan hasta el final de carrera real."""
        if not self.calibrada or not self.armado or self.estado in (ERROR, BUSCANDO, CALIBRANDO):
            return False
        L = config.EJE_LONGITUD_MM
        mm = max(0.0, min(L, mm))
        self._manual = 0
        if mm <= config.EJE_TOLERANCIA_MM:
            self._tipo = _A_INICIO
        elif mm >= L - config.EJE_TOLERANCIA_MM:
            self._tipo = _A_FIN
        else:
            self._tipo, self._destino = _A_POS, mm
        self._empezar_movimiento()
        return True

    def manual(self, velocidad):
        """Jog con el joystick (solo en LISTO o SIN_REF). 0 = parar."""
        self._manual = velocidad

    def parar(self):
        """Frena y cancela lo que esté en curso."""
        self._manual = 0
        self._frenar()
        if self.estado in (BUSCANDO, CALIBRANDO):
            self.calibrada = False
            self.estado = SIN_REF
        elif self.estado == MOVIENDO:
            self.estado = LISTO
        self._timeout = config.CARRO_TIMEOUT_MS

    def reiniciar_error(self):
        """Limpia el error; hay que referenciar de nuevo. False si la causa persiste."""
        self.finales.actualizar()
        if self.finales.inicio and self.finales.fin:
            return False
        self.error = ""
        self.estado = SIN_REF
        self.calibrada = False
        self._pedida = self._manual = 0
        self._mov = False
        self._timeout = config.CARRO_TIMEOUT_MS
        return True

    # ---- consultas -----------------------------------------------------
    @property
    def posicion_mm(self):
        """None si no está calibrado."""
        if not self.calibrada:
            return None
        if self._acum <= 0:
            return 0.0
        if self._acum >= self._total:
            return config.EJE_LONGITUD_MM
        return self._acum * config.EJE_LONGITUD_MM / self._total

    def quieto(self):
        """¿Parado de verdad? Medir el color bloquea ~270 ms sin vigilar los finales."""
        return (self.estado in (LISTO, SIN_REF, ERROR) and self._manual == 0
                and self.motor.real == 0)

    # ---- internos ------------------------------------------------------
    def _frenar(self):
        self._pedida = 0
        self._mov = False
        self.motor.frenar()

    def _fallar(self, texto):
        self._frenar()
        self.error = texto
        self.estado = ERROR
        self._timeout = config.CARRO_TIMEOUT_MS

    def _empezar_movimiento(self):
        # Tiempo máximo de un viaje: lo que tardó la calibración + 40 %.
        if self.calibrada:
            self._timeout = self.recorrido_ms * 7 // 5 + 500
        self.estado = MOVIENDO

    def _paso_buscando(self):
        f = self.finales
        if f.inicio:
            self._frenar()
            self._acum = 0
            self._t_cal = ticks_ms()
            self._timeout = config.EJE_RECORRIDO_ESPERADO_MS * 7 // 5
            self.estado = CALIBRANDO
        elif f.fin and not self._fin_previo:
            self._fallar("Sentido del motor invertido (MOTOR_SIGNO)")
        else:
            self._fin_previo = f.fin
            self._pedida = -config.EJE_VEL_HOMING

    def _paso_calibrando(self):
        if self.finales.fin:
            ms = diff(ticks_ms(), self._t_cal)
            self._frenar()
            if ms < config.EJE_RECORRIDO_MIN_MS or self._acum <= 0:
                self._fallar("Calibración demasiado corta (revisar finales)")
                return
            self._total, self.calibrada, self.recorrido_ms = self._acum, True, ms
            self._tipo = _A_INICIO              # vuelve al inicio y queda listo
            self._empezar_movimiento()
        else:
            self._pedida = config.EJE_VEL_VIAJE

    def _paso_moviendo(self):
        v = config.EJE_VEL_VIAJE
        if self._tipo == _A_INICIO:
            if self.finales.inicio:
                self._frenar()
                self.estado = LISTO
            else:
                self._pedida = -v
        elif self._tipo == _A_FIN:
            if self.finales.fin:
                self._frenar()
                self.estado = LISTO
            else:
                self._pedida = v
        else:
            resto = self._destino - self.posicion_mm
            if abs(resto) <= config.EJE_TOLERANCIA_MM:
                self._frenar()
                self.estado = LISTO
            else:
                self._pedida = v if resto > 0 else -v

    def actualizar(self):
        ahora = ticks_us()
        dt = diff(ahora, self._t_ult)
        self._t_ult = ahora
        f = self.finales
        f.actualizar()
        ini, fin = f.inicio, f.fin

        # Integra lo que el motor hace de verdad y resincroniza en los extremos.
        self._acum += self.motor.real * config.MOTOR_SIGNO * dt
        if ini:
            self._acum = 0

        if self.estado != ERROR and ini and fin:
            self._fallar("Los dos finales activos a la vez")

        if self.estado == BUSCANDO:
            self._paso_buscando()
        elif self.estado == CALIBRANDO:
            self._paso_calibrando()
        elif self.estado == MOVIENDO:
            self._paso_moviendo()
        elif self.estado in (LISTO, SIN_REF):
            self._pedida = self._manual

        if fin and self.calibrada:
            self._acum = self._total

        if self.estado == ERROR:
            self.motor.actualizar()
            return

        # Límites: con un extremo activo solo se permite alejarse de él.
        v = self._pedida
        permitida = 0 if (fin and v > 0) or (ini and v < 0) else v
        if v and not permitida and self.motor.real:
            self.motor.frenar()                 # extremo alcanzado en marcha: sin rampa
        self.motor.velocidad(permitida * config.MOTOR_SIGNO)

        # Tiempo máximo de movimiento continuo.
        if permitida:
            if not self._mov:
                self._mov, self._t_mov = True, ticks_ms()
            elif diff(ticks_ms(), self._t_mov) >= self._timeout:
                self._fallar("Tiempo máximo de movimiento excedido")
        else:
            self._mov = False

        self.motor.actualizar()
