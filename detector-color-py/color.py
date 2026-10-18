"""Lógica de color sin hardware: calibración, RGB -> HSV y color -> posición."""
import config

NEGRO, BLANCO, GRIS, CROMATICO = "negro", "blanco", "gris", "con tono"


def _canal(f, negro, blanco):
    """Frecuencia -> 0..255 con calibración de dos puntos."""
    if f <= negro:
        return 0
    if f >= blanco:
        return 255
    return ((f - negro) * 255 + (blanco - negro) // 2) // (blanco - negro)


def normalizar(f, negro, blanco):
    """f, negro, blanco = (r, g, b) en Hz. Devuelve (r, g, b) 0..255 o None."""
    if any(b <= n for n, b in zip(negro, blanco)):
        return None
    return tuple(_canal(f[i], negro[i], blanco[i]) for i in range(3))


def a_hex(rgb):
    return "#%02X%02X%02X" % rgb


def a_hsv(rgb):
    """Devuelve (h, s, v): h 0..359 (o -1 si es gris puro), s y v 0..255."""
    r, g, b = rgb
    mx, mn = max(r, g, b), min(r, g, b)
    d = mx - mn
    v = mx
    s = 0 if mx == 0 else d * 255 // mx
    if d == 0:
        return -1, s, v
    # int(x / d) trunca hacia cero, igual que la división entera de C
    if mx == r:
        h = int(60 * (g - b) / d)
    elif mx == g:
        h = 120 + int(60 * (b - r) / d)
    else:
        h = 240 + int(60 * (r - g) / d)
    return h % 360, s, v


def tono_a_rgb(h):
    """Color puro (S = V = 100 %) de un tono 0..359, para dibujar la escala."""
    sector, resto = divmod(h % 360, 60)
    t = resto * 255 // 60
    return ((255, t, 0), (255 - t, 255, 0), (0, 255, t),
            (0, 255 - t, 255), (t, 0, 255), (255, 0, 255 - t))[sector]


def tono_a_mm(h):
    return config.ESCALA_DESPLAZAMIENTO_MM + (h % 360) * config.ESCALA_ANCHO_MM / 360


def mapear(hsv, actual_mm=None):
    """Clasifica el color y calcula su posición en la escala.

    Devuelve (clase, x_mm); x_mm es None para negro/gris/blanco (el cursor
    no se mueve). Cerca del rojo, que está impreso en los dos extremos, se
    elige el extremo más próximo a `actual_mm` para no saltar de lado a lado.
    """
    h, s, v = hsv
    if v < config.ESCALA_VAL_NEGRO:
        return NEGRO, None
    if h < 0 or s < config.ESCALA_SAT_MIN:
        return (BLANCO if v >= config.ESCALA_VAL_BLANCO else GRIS), None

    x = tono_a_mm(h)
    if actual_mm is not None and (h < config.ESCALA_BANDA_ROJO or h > 360 - config.ESCALA_BANDA_ROJO):
        otro = config.ESCALA_DESPLAZAMIENTO_MM + (config.ESCALA_ANCHO_MM if h < 180 else 0)
        if abs(actual_mm - otro) < abs(actual_mm - x):
            x = otro
    return CROMATICO, x


def hay_que_mover(actual_mm, objetivo_mm):
    return actual_mm is None or abs(actual_mm - objetivo_mm) > config.ESCALA_ZONA_MUERTA_MM
