#!/usr/bin/env python3
"""Genera docs/conexiones.md (tabla de cableado) a partir de src/configuracion/pines.h.

pines.h es la fuente de verdad: este script solo lo traduce a números de pin físico y a
tablas por módulo, para que el cableado no se desincronice del firmware.

Uso:  python tools/generar_conexiones.py
"""
import os
import re
import sys

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PINES_H = os.path.join(RAIZ, "src", "configuracion", "pines.h")
SALIDA = os.path.join(RAIZ, "docs", "conexiones.md")

# Pico / Pico W: GPIO -> número de pin físico del conector de 40 pines (es el mismo en las dos placas).
FISICO = {0: 1, 1: 2, 2: 4, 3: 5, 4: 6, 5: 7, 6: 9, 7: 10, 8: 11, 9: 12, 10: 14, 11: 15, 12: 16, 13: 17,
          14: 19, 15: 20, 16: 21, 17: 22, 18: 24, 19: 25, 20: 26, 21: 27, 22: 29, 26: 31, 27: 32, 28: 34}

# Comprobación cruzada con la tabla que ya se verificó en el hardware de Lunar Lander.
VERIFICADO_LUNAR_LANDER = {18: 24, 19: 25, 17: 22, 16: 21, 20: 26, 26: 31, 27: 32, 22: 29, 15: 20}

# Pines del conector que no son GPIO (para el diagrama).
OTROS = {3: "GND", 8: "GND", 13: "GND", 18: "GND", 23: "GND", 28: "GND", 33: "AGND", 38: "GND",
         40: "VBUS", 39: "VSYS", 37: "3V3_EN", 36: "3V3(OUT)", 35: "ADC_VREF", 30: "RUN"}
RESERVADOS_WIFI = {23: "WL_ON", 24: "WL_D", 25: "WL_CS", 29: "WL_CLK"}   # GPIO del CYW43 (no tienen pin en el conector)


def leer_pines():
    pines = {}
    for linea in open(PINES_H, encoding="utf-8"):
        m = re.match(r"\s*#define\s+(PIN_[A-Z0-9_]+)\s+(\d+)u?\b", linea)
        if m:
            pines[m.group(1)] = int(m.group(2))
    return pines


P = leer_pines()


def gp(nombre):
    return P[nombre]


def destino(nombre):
    n = gp(nombre)
    return "**GP%d** (pin %d)" % (n, FISICO[n])


def comprobar():
    for g, f in VERIFICADO_LUNAR_LANDER.items():
        assert FISICO[g] == f, "pin físico de GP%d no coincide con Lunar Lander" % g
    usados = sorted(P.values())
    assert len(usados) == len(set(usados)), "hay un GPIO repetido en pines.h"
    for g in usados:
        assert g in FISICO, "GP%d no tiene pin físico (¿reservado por el Wi-Fi?)" % g
    assert len(set(FISICO.values())) == len(FISICO), "dos GPIO con el mismo pin físico"
    assert not (set(usados) & set(RESERVADOS_WIFI)), "se usa un GPIO reservado por el CYW43"


# (módulo, [(pin del módulo, destino, nota)], aviso)
def modulos():
    return [
        ("Pantalla ST7789 240×240 (sin CS)", [
            ("GND", "GND común", ""),
            ("VCC", "3V3 de la fuente externa", ""),
            ("SCK", destino("PIN_TFT_SCK"), "SPI0 SCK"),
            ("SDA", destino("PIN_TFT_MOSI"), "es MOSI (SPI0 TX), no I2C"),
            ("RES", destino("PIN_TFT_RST"), ""),
            ("DC", destino("PIN_TFT_DC"), ""),
            ("BLK", "GND", "backlight activo en bajo: cableado fijo, siempre encendido"),
        ], "Mismo cableado verificado en Lunar Lander. El módulo no tiene pin CS."),

        ("Sensor de color TCS3200", [
            ("VCC", "3V3 de la fuente externa", ""),
            ("GND", "GND común", ""),
            ("S0", "3V3 (fijo)", "S0=H, S1=L → escala 20 %. HIPÓTESIS"),
            ("S1", "GND (fijo)", "no ocupa GPIO"),
            ("OE", "GND (fijo)", "salida siempre habilitada"),
            ("S2", destino("PIN_TCS_S2"), "selección de filtro"),
            ("S3", destino("PIN_TCS_S3"), "selección de filtro"),
            ("OUT", destino("PIN_TCS_OUT"), "debe ir a un GPIO IMPAR: lo cuenta el PWM2 canal B"),
        ], "Si el módulo trae pin LED o puente para los LED blancos, déjalo conectado (encendidos siempre)."),

        ("IR final de carrera INICIO", [
            ("VCC", "3V3 de la fuente externa", ""),
            ("GND", "GND común", ""),
            ("OD", destino("PIN_IR_INICIO"), "salida digital. PENDIENTE confirmar cuál de OA/OD es la digital"),
            ("OA", "sin conectar", ""),
        ], ""),

        ("IR final de carrera FIN", [
            ("VCC", "3V3 de la fuente externa", ""),
            ("GND", "GND común", ""),
            ("OD", destino("PIN_IR_FIN"), "salida digital. PENDIENTE confirmar cuál de OA/OD es la digital"),
            ("OA", "sin conectar", ""),
        ], ""),

        ("IR de proximidad (junto al TCS3200)", [
            ("VCC", "3V3 de la fuente externa", ""),
            ("GND", "GND común", ""),
            ("OA", destino("PIN_IR_PROX_A"), "analógica → ADC2 (distancia). HIPÓTESIS"),
            ("OD", destino("PIN_IR_PROX_D"), "digital (hay objeto). HIPÓTESIS"),
        ], "Polaridades y umbrales se miden con PRUEBA_PROXIMIDAD (docs/hardware.md §10)."),

        ("Driver de motor TB6612FNG + motor DC", [
            ("VM", "alimentación del MOTOR (fuente aparte)", "NO del 3V3: el TB6612 pide ≈ 4,5–13,5 V (verificar en su datasheet)"),
            ("VCC", "3V3 de la fuente externa", "lógica"),
            ("GND", "GND común", "obligatoria: la masa del motor y la de la lógica deben unirse"),
            ("STBY", destino("PIN_MOTOR_STBY"), "además, resistencia de 10 kΩ a GND (ver «Componentes»)"),
            ("PWMA", destino("PIN_MOTOR_PWMA"), "PWM4 canal A, 20 kHz"),
            ("AIN1", destino("PIN_MOTOR_AIN1"), "sentido"),
            ("AIN2", destino("PIN_MOTOR_AIN2"), "sentido"),
            ("AO1 / AO2", "los dos cables del motor", "si gira al revés: invertir en MOTOR_SIGNO_HACIA_FIN"),
            ("BIN1, BIN2, PWMB", "GND", "canal B sin usar"),
            ("BO1 / BO2", "sin conectar", ""),
        ], "Probar SIEMPRE con la tarea de prueba del motor y el corte de alimentación a mano."),

        ("LED RGB de ÁNODO COMÚN", [
            ("Ánodo (+)", "3V3 de la fuente externa", "pata más larga"),
            ("R", "resistencia 150 Ω → " + destino("PIN_LED_R"), "PWM5 canal B"),
            ("G", "resistencia 47 Ω → " + destino("PIN_LED_G"), "PWM6 canal A"),
            ("B", "resistencia 47 Ω → " + destino("PIN_LED_B"), "PWM6 canal B"),
        ], "Resistencias orientativas (≤ 12 mA por pin): ajustar a ojo hasta que el blanco se vea blanco."),

        ("Buzzer pasivo — opción A: directo", [
            ("+ (señal)", destino("PIN_BUZZER"), "PWM7 canal B"),
            ("− (GND)", "GND común", ""),
        ], "Solo si el buzzer es piezoeléctrico y consume poco (un GPIO da ≈ 12 mA)."),

        ("Buzzer pasivo — opción B: con transistor NPN", [
            ("GPIO", destino("PIN_BUZZER") + " → 1 kΩ → base del NPN", "S8050 / 2N2222 / BC547"),
            ("Emisor", "GND común", ""),
            ("Colector", "pata − del buzzer", ""),
            ("Pata + del buzzer", "3V3 de la fuente externa", "diodo 1N4148 en paralelo si el buzzer es magnético"),
        ], "Recomendada si el buzzer suena bajo o la Pico se reinicia al sonar."),

        ("Botones (pulsadores, uno por función)", [
            ("B1 · iniciar detección", destino("PIN_BOTON_1") + " ↔ GND", "un terminal al GPIO, el otro a GND"),
            ("B2 · calibrar", destino("PIN_BOTON_2") + " ↔ GND", "pull-up interno"),
            ("B3 · posicionar", destino("PIN_BOTON_3") + " ↔ GND", "pull-up interno"),
            ("B4 · guardar / menú", destino("PIN_BOTON_4") + " ↔ GND", "pull-up interno"),
        ], "Activos en bajo. Con cables largos y pulsaciones fantasma: 100 nF entre el pin y GND."),

        ("Joystick analógico (tipo KY-023)", [
            ("VCC (+5V)", "3V3 de la fuente externa", "NUNCA 5 V: los GPIO no lo toleran"),
            ("GND", "GND común", ""),
            ("VRx / JX", destino("PIN_JOY_X"), "ADC0"),
            ("VRy / JY", destino("PIN_JOY_Y"), "ADC1"),
            ("SW", destino("PIN_JOY_SW"), "pulsador; pull-up interno"),
        ], "Mantener el joystick quieto al arrancar: mide su centro en ese momento."),
    ]


def funcion_gpio():
    """Función de cada GPIO, para la tabla ordenada por pin."""
    return {
        "PIN_BOTON_4": ("Botón 4 · guardar/menú", "entrada, pull-up"),
        "PIN_IR_PROX_D": ("IR proximidad · OD", "entrada"),
        "PIN_TCS_S2": ("TCS3200 · S2", "salida"),
        "PIN_TCS_S3": ("TCS3200 · S3", "salida"),
        "PIN_IR_INICIO": ("IR final INICIO", "entrada"),
        "PIN_TCS_OUT": ("TCS3200 · OUT", "PWM2 B (contador)"),
        "PIN_IR_FIN": ("IR final FIN", "entrada"),
        "PIN_MOTOR_STBY": ("TB6612 · STBY", "salida"),
        "PIN_MOTOR_PWMA": ("TB6612 · PWMA", "PWM4 A"),
        "PIN_MOTOR_AIN1": ("TB6612 · AIN1", "salida"),
        "PIN_MOTOR_AIN2": ("TB6612 · AIN2", "salida"),
        "PIN_LED_R": ("LED RGB · R", "PWM5 B"),
        "PIN_LED_G": ("LED RGB · G", "PWM6 A"),
        "PIN_LED_B": ("LED RGB · B", "PWM6 B"),
        "PIN_BOTON_1": ("Botón 1 · iniciar detección", "entrada, pull-up"),
        "PIN_BUZZER": ("Buzzer", "PWM7 B"),
        "PIN_TFT_DC": ("ST7789 · DC", "salida"),
        "PIN_TFT_RST": ("ST7789 · RES", "salida"),
        "PIN_TFT_SCK": ("ST7789 · SCK", "SPI0 SCK"),
        "PIN_TFT_MOSI": ("ST7789 · SDA", "SPI0 TX"),
        "PIN_BOTON_2": ("Botón 2 · calibrar", "entrada, pull-up"),
        "PIN_BOTON_3": ("Botón 3 · posicionar", "entrada, pull-up"),
        "PIN_JOY_SW": ("Joystick · SW", "entrada, pull-up"),
        "PIN_JOY_X": ("Joystick · JX", "ADC0"),
        "PIN_JOY_Y": ("Joystick · JY", "ADC1"),
        "PIN_IR_PROX_A": ("IR proximidad · OA", "ADC2"),
    }


def resumen_filas():
    """Todas las conexiones en filas planas: (módulo, pin del módulo, conecta a, pin físico de la Pico)."""
    patron = re.compile(r"[*][*]GP([0-9]+)[*][*] [(]pin ([0-9]+)[)]")
    filas = []
    for nombre, conexiones, _aviso in modulos():
        for pin, dest, _nota in conexiones:
            m = patron.search(dest)
            limpio = dest.replace("**", "")
            fisico = m.group(2) if m else "—"
            filas.append((nombre, pin, limpio, fisico))
    return filas


def construir():
    comprobar()
    L = []
    w = L.append
    w("# Tabla de conexiones — Raspberry Pi Pico W ↔ sensores y periféricos")
    w("")
    w("> **GENERADO** por `tools/generar_conexiones.py` a partir de `src/configuracion/pines.h`. No editar a mano:")
    w("> cambia `pines.h` y vuelve a ejecutar el script. Los números de pin físico son los del conector de")
    w("> 40 pines de la Pico / Pico W y se han cruzado con la tabla verificada de Lunar Lander.")
    w(">")
    w("> **Estado:** cableado propuesto, **sin comprobar con multímetro ni con el hardware**. Marcado")
    w("> «HIPÓTESIS» o «PENDIENTE» lo que depende de datos que aún no se han medido.")
    w("")
    w("## Resumen: todas las conexiones en una tabla")
    w("")
    w("| Módulo | Pin del módulo | Se conecta a | Pin físico de la Pico |")
    w("|---|---|---|:---:|")
    anterior = None
    for modulo, pin, dest, fisico in resumen_filas():
        etiqueta = "**%s**" % modulo if modulo != anterior else ""
        anterior = modulo
        w("| %s | %s | %s | %s |" % (etiqueta, pin, dest, fisico))
    w("")
    w("«Pin físico» es el número del conector de 40 pines de la Pico W; «—» indica que no va a la Pico")
    w("(alimentación, masa, componentes externos o conexiones fijas). Los detalles y las notas de cada")
    w("módulo están en la sección 2.")
    w("")
    w("> ⚠ **El joystick está serigrafiado «+5V»: conéctalo a 3V3, nunca a 5 V** (los GPIO no lo toleran).")
    w("> ⚠ **`VM` del TB6612 no puede salir del 3V3**: necesita su propia fuente (≈ 4,5–13,5 V según el datasheet de Toshiba; verificarlo),")
    w("> con la GND unida al GND común.")
    w("")
    w("## 1. Alimentación y masas")
    w("")
    w("| De | A | Nota |")
    w("|---|---|---|")
    w("| Fuente externa **+3V3** | VCC de **todos** los módulos (ST7789, TCS3200, 3 IR, TB6612 lógica, joystick, ánodo del LED) | un solo bus de 3V3 |")
    w("| Fuente externa **GND** | GND de todos los módulos **y** un GND de la Pico (pin 3, 8, 13, 18, 23, 28 o 38) | **masa común obligatoria** |")
    w("| Fuente del **motor** (+) | **VM** del TB6612FNG | fuente aparte; su **GND también va al GND común** |")
    w("| Pico W | **USB**, o bien **VSYS (pin 39) a través de un diodo Schottky** desde la fuente externa | VSYS admite 1,8–5,5 V |")
    w("| ❌ **NO conectar** | el +3V3 de la fuente al pin **36 (3V3 OUT)** de la Pico | pondría dos fuentes en paralelo |")
    w("")
    w("Enciende la Pico y los módulos a la vez, o la Pico primero: un módulo alimentado con la Pico apagada")
    w("empuja corriente por sus salidas hacia GPIO sin alimentar.")
    w("")
    w("## 2. Conexiones por módulo")
    w("")
    for i, (nombre, filas, aviso) in enumerate(modulos(), start=1):
        w("### 2.%d %s" % (i, nombre))
        w("")
        w("| Pin del módulo | Se conecta a | Nota |")
        w("|---|---|---|")
        for pin, dest, nota in filas:
            w("| %s | %s | %s |" % (pin, dest, nota))
        if aviso:
            w("")
            w("> %s" % aviso)
        w("")

    w("## 3. Mapa por GPIO (para cablear siguiendo el conector)")
    w("")
    w("| Pin físico | GPIO | Señal | Función |")
    w("|---:|---|---|---|")
    desc = funcion_gpio()
    por_gpio = {g: desc[n] for n, g in P.items()}
    for g in sorted(FISICO, key=lambda x: FISICO[x]):
        if g in por_gpio:
            w("| %d | GP%d | %s | %s |" % (FISICO[g], g, por_gpio[g][0], por_gpio[g][1]))
    w("")
    w("Quedan **todos los GPIO utilizables asignados** (GP0–GP22 y GP26–GP28). **No usar** GP23, GP24, GP25 ni GP29:")
    w("los ocupa el chip Wi-Fi (CYW43) de la Pico W. Los pines 3, 8, 13, 18, 23, 28, 33 y 38 son GND.")
    w("")
    w("## 4. Componentes externos recomendados")
    w("")
    w("| Dónde | Componente | Para qué |")
    w("|---|---|---|")
    w("| TB6612 `STBY` ↔ GND | resistencia **10 kΩ** | mantiene el puente apagado mientras el GPIO está sin configurar (arranque) |")
    w("| TB6612 `VM` ↔ GND | electrolítico **≥ 470 µF** + cerámico 100 nF, lo más cerca del módulo | picos de arranque del motor |")
    w("| Junto a cada módulo (VCC ↔ GND) | cerámico **100 nF** | ruido del motor sobre el TCS3200 y el ADC |")
    w("| LED RGB | **150 Ω** (R), **47 Ω** (G y B) | limitar la corriente a 3,3 V |")
    w("| Fuente → VSYS de la Pico | diodo **Schottky** | evita retroalimentar a la fuente cuando se conecta el USB |")
    w("")
    w("## 5. Diagrama del conector (Pico W vista desde arriba, USB arriba)")
    w("")
    w("```")
    w("                 +--USB--+")
    inverso = {f: g for g, f in FISICO.items()}
    descripciones = {g: desc[n][0] for n, g in P.items()}

    def lado(pin):
        if pin in inverso:
            g = inverso[pin]
            return "GP%d" % g, descripciones.get(g, "(libre)")
        return OTROS[pin], ""

    for i in range(20):
        a, b = i + 1, 40 - i
        na, da = lado(a)
        nb, db = lado(b)
        izq = ("%s %s" % (da, na)).strip() if da else na
        der = ("%s %s" % (nb, db)).strip() if db else nb
        w("%34s %2d |      | %-2d %s" % (izq, a, b, der))
    w("```")
    w("")
    w("## 6. Antes de dar corriente")
    w("")
    w("1. Comprobar con multímetro que **no hay continuidad entre +3V3 y GND**.")
    w("2. Comprobar que **GND de la fuente, de los módulos, de la fuente del motor y de la Pico** están unidos.")
    w("3. Con el motor **desconectado**, cargar la prueba de los finales de carrera y verificar los IR a mano")
    w("   (`prueba_motor.c`, paso 0). Después, el pulso corto de prueba del motor.")
    w("4. Verificar la tensión de cada pin de alimentación de módulo: **3,3 V**, nunca 5 V.")
    w("")
    return "\n".join(L) + "\n"


def main():
    texto = construir()
    os.makedirs(os.path.dirname(SALIDA), exist_ok=True)
    with open(SALIDA, "w", encoding="utf-8", newline="\n") as f:
        f.write(texto)
    print("Escrito %s (%d GPIO asignados, sin conflictos)" % (os.path.normpath(SALIDA), len(P)))


main()
