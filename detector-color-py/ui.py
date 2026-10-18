"""Interfaz de la pantalla: principal, menú, historial, mover cursor e información.

No toca sensores ni motor: recibe un `Modelo` (copia del estado) y una `Entrada`
(botones ya interpretados) y devuelve una orden que la aplicación ejecuta.
dibujar() pinta UNA sección por llamada para que el bucle principal siga vigilando
el motor y los finales de carrera; refresca toda la pantalla en unas 6 llamadas.
"""
import color
import config
from pantalla import ANCHO, rgb565, NEGRO, BLANCO

# ---- Paleta ----
C_FONDO = rgb565(14, 16, 22)
C_PANEL = rgb565(32, 36, 48)
C_CHIP = rgb565(55, 60, 75)
C_TEXTO = rgb565(235, 235, 240)
C_TENUE = rgb565(135, 140, 152)
C_OK = rgb565(70, 200, 120)
C_AVISO = rgb565(240, 175, 50)
C_ERROR = rgb565(235, 75, 75)
C_ACENTO = rgb565(80, 155, 245)
C_SEL = rgb565(45, 85, 160)
C_R, C_G, C_B = rgb565(245, 95, 95), rgb565(95, 225, 125), rgb565(105, 165, 255)

MARGEN = 6
ANCHO_UTIL = ANCHO - 2 * MARGEN       # 228
FILA = 20

PRINCIPAL, MENU, HISTORIAL, MOVER, INFO = range(5)
ITEMS = (("Medir color", "medir"), ("Calibrar negro", "cal_negro"), ("Calibrar blanco", "cal_blanco"),
         ("Referenciar eje", "referenciar"), ("Mover cursor", "mover"), ("Modo manual", "manual"),
         ("Historial", "historial"), ("Armar motor", "armar"), ("Información", "info"))


class Modelo:
    """Lo que la interfaz muestra. La aplicación lo rellena en cada ciclo."""
    estado = "INICIO"
    estado_color = C_TENUE
    armado = ref = cal = False
    lectura = None                  # (rgb, hex, hsv, clase) de la última medición
    objetivo_mm = None
    pos_mm = None
    eje = ""
    ir_inicio = ir_fin = False
    prox = "--"
    mensaje = ""
    error = ""
    hay_negro = hay_blanco = False
    recorrido_ms = 0
    historial = ()                  # más reciente al final: (rgb, h, x_mm)


class Entrada:
    b1 = b2 = b3 = b4 = b4_largo = ok = False
    dir = 0                         # -1 arriba, +1 abajo (un pulso por movimiento)
    joy_x = 0                       # -100..100, continuo


def _mm(v):
    return "---.-" if v is None else "%.1f" % v


def _contraste(rgb):
    return NEGRO if (rgb[0] * 299 + rgb[1] * 587 + rgb[2] * 114) // 1000 > 128 else BLANCO


class Interfaz:
    def __init__(self, pantalla):
        self.p = pantalla
        self.pantalla = PRINCIPAL
        self._nueva = True
        self._seccion = 0
        self._menu = 0
        self._hist = 0
        self._mover_mm = 0.0
        self._marca_obj = self._marca_pos = -1
        self._escala = None         # fila de píxeles de la escala (se calcula una vez)

    def _ir(self, pantalla):
        self.pantalla, self._nueva, self._seccion = pantalla, True, 0

    # ==================================================================
    # Navegación: entradas -> orden
    # ==================================================================
    def procesar(self, e, m):
        """Devuelve None o una orden: 'medir', 'cal_negro', 'cal_blanco', 'referenciar',
        'guardar', 'armar', 'desarmar', 'manual' o ('ir_a', mm)."""
        p = self.pantalla
        salir = e.b4 or e.b4_largo
        if p == PRINCIPAL:
            if e.b1:
                return "medir"
            if e.b2:
                self._menu = 1
                self._ir(MENU)
            elif e.b3:
                self._entrar_mover(m)
            elif e.b4:
                return "guardar"
            elif e.b4_largo or e.ok:
                self._menu = 0
                self._ir(MENU)
        elif p == MENU:
            if e.dir:
                self._menu = (self._menu + e.dir) % len(ITEMS)
            if salir:
                self._ir(PRINCIPAL)
            elif e.ok:
                return self._elegir(ITEMS[self._menu][1], m)
        elif p == HISTORIAL:
            n = len(m.historial)
            if e.dir and n:
                self._hist = (self._hist + e.dir) % n
            if salir:
                self._ir(PRINCIPAL)
            elif e.ok and n:
                x = m.historial[n - 1 - self._hist][2]       # 0 = la más reciente
                if x is not None:
                    self._ir(PRINCIPAL)
                    return ("ir_a", x)
        elif p == MOVER:
            self._mover_mm = max(0.0, min(config.EJE_LONGITUD_MM, self._mover_mm + e.joy_x * 0.04))
            if salir:
                self._ir(PRINCIPAL)
            elif e.ok:
                self._ir(PRINCIPAL)
                return ("ir_a", self._mover_mm)
        elif p == INFO and (salir or e.ok):
            self._ir(PRINCIPAL)
        return None

    def _entrar_mover(self, m):
        self._mover_mm = m.pos_mm or 0.0
        self._ir(MOVER)

    def _elegir(self, clave, m):
        if clave == "mover":
            self._entrar_mover(m)
            return None
        if clave == "historial":
            self._hist = 0
            self._ir(HISTORIAL)
            return None
        if clave == "info":
            self._ir(INFO)
            return None
        self._ir(PRINCIPAL)
        return ("desarmar" if m.armado else "armar") if clave == "armar" else clave

    # ==================================================================
    # Dibujo
    # ==================================================================
    def dibujar(self, m):
        p = self.p
        if self._nueva:                                    # cambiar de pantalla borra todo de una vez
            self._nueva = False
            p.rellenar(C_FONDO)
            if self.pantalla != PRINCIPAL:
                titulo = ("", "Menú", "Historial", "Mover cursor", "Información")[self.pantalla]
                p.rect(0, 0, ANCHO, 22, C_PANEL)
                p.texto(MARGEN, 3, titulo, 2, C_TEXTO, C_PANEL)
            if self.pantalla in (PRINCIPAL, MOVER):
                self._escala_estatica()
            self._seccion = 0
            return
        total = (self._principal, self._menu_s, self._historial_s, self._mover_s, self._info_s)[self.pantalla](m, self._seccion)
        self._seccion = (self._seccion + 1) % total

    def _pie(self, texto):
        self.p.texto(MARGEN, 226, texto, 1, C_TENUE, C_FONDO, 38)

    def _escala_estatica(self):
        if self._escala is None:
            linea = bytearray(ANCHO_UTIL * 2)
            for x in range(ANCHO_UTIL):
                r, g, b = color.tono_a_rgb(x * 360 // ANCHO_UTIL)
                c = rgb565(r, g, b)
                linea[2 * x], linea[2 * x + 1] = c >> 8, c & 255
            self._escala = bytes(linea)
        y, alto = (184, 14) if self.pantalla == PRINCIPAL else (150, 20)
        for i in range(alto):
            self.p.bloque(MARGEN, y + i, ANCHO_UTIL, 1, self._escala)
        self._marca_obj = self._marca_pos = -1

    def _px(self, mm):
        px = int((mm - config.ESCALA_DESPLAZAMIENTO_MM) * ANCHO_UTIL / config.ESCALA_ANCHO_MM)
        return MARGEN + max(0, min(ANCHO_UTIL - 1, px))

    def _marca(self, atributo, mm, y, alto, col):
        """Marca vertical; borra la anterior solo si cambió de columna."""
        px = -1 if mm is None else self._px(mm)
        anterior = getattr(self, atributo)
        if px == anterior:
            return
        if anterior >= 0:
            self.p.rect(anterior - 1, y, 3, alto, C_FONDO)
        if px >= 0:
            self.p.rect(px - 1, y, 3, alto, col)
        setattr(self, atributo, px)

    def _chip(self, x, y, texto, activo, col):
        w = len(texto) * 6 + 5
        fondo = col if activo else C_CHIP
        self.p.rect(x, y, w, 11, fondo)
        self.p.texto(x + 3, y + 2, texto, 1, C_FONDO if activo else C_TENUE, fondo)

    def _principal(self, m, s):
        p = self.p
        if s == 0:                                         # barra de estado
            p.rect(0, 0, ANCHO, 22, C_PANEL)
            p.texto(MARGEN, 3, m.estado, 2, m.estado_color, C_PANEL)
            x = ANCHO - 4
            for nombre, activo in (("ARM", m.armado), ("REF", m.ref), ("CAL", m.cal)):
                x -= len(nombre) * 6 + 8
                self._chip(x, 6, nombre, activo, C_OK)
        elif s == 1:                                       # muestra del color
            if m.lectura:
                rgb, hx, _, clase = m.lectura
                c, t = rgb565(*rgb), _contraste(rgb)
                p.rect(MARGEN, 24, ANCHO_UTIL, 72, c)
                p.texto_centrado(MARGEN, ANCHO_UTIL, 42, hx, 3, t, c)
                p.texto_centrado(MARGEN, ANCHO_UTIL, 80, clase if clase == color.CROMATICO else clase + " (sin tono)", 1, t, c)
            else:
                p.rect(MARGEN, 24, ANCHO_UTIL, 72, C_PANEL)
                p.texto_centrado(MARGEN, ANCHO_UTIL, 44, "SIN LECTURA", 2, C_TENUE, C_PANEL)
                p.texto_centrado(MARGEN, ANCHO_UTIL, 70, "pulsa B1 para medir", 1, C_TENUE, C_PANEL)
        elif s == 2:                                       # R G B y H S V
            if m.lectura:
                (r, g, b), _, (h, sat, v), _ = m.lectura
                p.texto(MARGEN, 100, "R:%3d" % r, 2, C_R, C_FONDO, 5)
                p.texto(MARGEN + 84, 100, "G:%3d" % g, 2, C_G, C_FONDO, 5)
                p.texto(MARGEN + 168, 100, "B:%3d" % b, 2, C_B, C_FONDO, 5)
                p.texto(MARGEN, 120, "H:%3d°" % h if h >= 0 else "H:---", 2, C_TEXTO if h >= 0 else C_TENUE, C_FONDO, 6)
                p.texto(MARGEN + 84, 120, "S:%d%%" % (sat * 100 // 255), 2, C_TEXTO, C_FONDO, 6)
                p.texto(MARGEN + 168, 120, "V:%d%%" % (v * 100 // 255), 2, C_TEXTO, C_FONDO, 6)
            else:
                for i, t in enumerate(("R", "G", "B")):
                    p.texto(MARGEN + 84 * i, 100, t + ":---", 2, C_TENUE, C_FONDO, 5)
                for i, t in enumerate(("H", "S", "V")):
                    p.texto(MARGEN + 84 * i, 120, t + ":---", 2, C_TENUE, C_FONDO, 6)
        elif s == 3:                                       # posición y objetivo
            p.texto(MARGEN, 140, "Pos %s mm" % _mm(m.pos_mm), 2, C_TEXTO, C_FONDO, 19)
            if m.objetivo_mm is not None:
                p.texto(MARGEN, 158, "Obj %s mm" % _mm(m.objetivo_mm), 2, C_ACENTO, C_FONDO, 19)
            else:
                p.texto(MARGEN, 158, "Obj ---", 2, C_TENUE, C_FONDO, 19)
        elif s == 4:                                       # marcas sobre la escala
            self._marca("_marca_obj", m.objetivo_mm, 176, 7, C_TEXTO)
            self._marca("_marca_pos", m.pos_mm, 199, 7, C_AVISO)
        else:                                              # eje, sensores, ayuda y mensaje
            p.texto(MARGEN, 210, "Eje:%s  IR:%s%s  Prox:%s" % (
                m.eje, "I" if m.ir_inicio else "-", "F" if m.ir_fin else "-", m.prox), 1, C_TENUE, C_FONDO, 38)
            p.texto(MARGEN, 222, "B1 Medir B2 Calib B3 Mover B4 Guardar", 1, C_TENUE, C_FONDO, 38)
            if m.error:
                p.texto(MARGEN, 232, m.error, 1, C_ERROR, C_FONDO, 38)
            elif m.mensaje:
                p.texto(MARGEN, 232, m.mensaje, 1, C_ACENTO, C_FONDO, 38)
            else:
                p.texto(MARGEN, 232, "SW o B4 largo: menú", 1, C_TENUE, C_FONDO, 38)
        return 6

    def _menu_s(self, m, s):
        n = len(ITEMS)
        if s >= n:
            self._pie("Joystick: elegir  SW: ok  B4: salir")
            return n + 1
        sel = s == self._menu
        fondo = C_SEL if sel else C_FONDO
        nombre = ITEMS[s][0]
        if ITEMS[s][1] == "armar" and m.armado:
            nombre = "Desarmar motor"
        self.p.rect(0, 30 + s * FILA, ANCHO, FILA, fondo)
        self.p.texto(10, 32 + s * FILA, "%s %s" % (">" if sel else " ", nombre), 2,
                     C_TEXTO if sel else C_TENUE, fondo)
        return n + 1

    def _historial_s(self, m, s):
        filas, n = 7, len(m.historial)
        if s >= filas:
            self._pie("SW: ir a esa posición  B4: volver" if n else "B4: volver")
            return filas + 1
        if not n:
            if s == 2:
                self.p.texto_centrado(0, ANCHO, 80, "Sin lecturas", 2, C_TENUE, C_FONDO)
                self.p.texto_centrado(0, ANCHO, 106, "guarda con B4 al medir", 1, C_TENUE, C_FONDO)
            return filas + 1
        primera = 0 if self._hist < filas else self._hist - (filas - 1)    # ventana que contiene la elegida
        idx, y = primera + s, 30 + s * FILA
        if idx >= n:
            self.p.rect(0, y, ANCHO, FILA, C_FONDO)
            return filas + 1
        rgb, _, x = m.historial[n - 1 - idx]
        sel = idx == self._hist
        fondo = C_SEL if sel else C_FONDO
        col = C_TEXTO if sel else C_TENUE
        self.p.rect(0, y, ANCHO, FILA, fondo)
        self.p.rect(8, y + 2, 16, 16, rgb565(*rgb))
        self.p.texto(32, y + 2, color.a_hex(rgb), 2, col, fondo)
        self.p.texto(128, y + 2, "%smm" % _mm(x) if x is not None else "sin pos.", 2,
                     col if x is not None else C_TENUE, fondo, 7)
        return filas + 1

    def _mover_s(self, m, s):
        if s == 0:
            self.p.texto(MARGEN, 52, _mm(self._mover_mm), 4, C_TEXTO, C_FONDO, 5)
            self.p.texto(MARGEN + 126, 68, "mm", 2, C_TENUE, C_FONDO)
        elif s == 1:
            self.p.texto(MARGEN, 100, "Actual %s mm" % _mm(m.pos_mm), 2, C_TENUE, C_FONDO, 19)
        elif s == 2:
            self._marca("_marca_obj", self._mover_mm, 134, 14, C_TEXTO)
            self._marca("_marca_pos", m.pos_mm, 172, 14, C_AVISO)
        else:
            self._pie("Joystick X: ajustar  SW: ir  B4: no")
        return 4

    def _info_s(self, m, s):
        lineas = (
            ("Color negro:   " + ("calibrado" if m.hay_negro else "pendiente"), C_TEXTO),
            ("Color blanco:  " + ("calibrado" if m.hay_blanco else "pendiente"), C_TEXTO),
            ("Recorrido:     " + ("%d ms (%d mm)" % (m.recorrido_ms, config.EJE_LONGITUD_MM)
                                  if m.recorrido_ms else "sin calibrar"), C_TEXTO),
            ("Eje:           " + m.eje, C_TEXTO),
            ("Finales IR:    inicio %s, fin %s" % ("ACTIVO" if m.ir_inicio else "libre",
                                                   "ACTIVO" if m.ir_fin else "libre"), C_TEXTO),
            ("IR proximidad: " + m.prox, C_TEXTO),
            (m.error or "Sin errores", C_ERROR if m.error else C_OK),
            ("Detector de color - MicroPython (RP2040)", C_TENUE),
        )
        if s < len(lineas):
            texto, col = lineas[s]
            self.p.texto(MARGEN, 32 + s * 14, texto, 1, col, C_FONDO, 38)
        else:
            self._pie("B4 o SW: volver")
        return len(lineas) + 1
