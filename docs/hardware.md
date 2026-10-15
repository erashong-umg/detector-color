# Hardware — Detector de color con cursor cromático lineal

Estado al 2026-10-14. Las asignaciones de pines viven en código en
`src/configuracion/pines.h`; si este documento y ese archivo difieren, manda
el archivo (tiene validación en compilación).

## 1. Hechos verificados

| Hecho | Fuente |
|---|---|
| Placa: **Raspberry Pi Pico W** (RP2040 + CYW43439) | Confirmado por el usuario |
| GP23 (WL_ON), GP24 (WL_D), GP25 (WL_CS), GP29 (WL_CLK / ADC3 = VSYS/3) los usa el chip Wi-Fi | Datasheet Pico W |
| ADC disponible en la Pico W: solo **GP26, GP27, GP28** (AIN0–2) | Datasheet Pico W |
| ST7789 = módulo de Lunar Lander: **240×240, sin CS**, pines GND/VCC/SCK/SDA/RES/DC/BLK | Probado en `Lunar lader/` |
| ST7789: SPI **modo 3**, **INVON**, MADCTL `0x00`, sin offsets, 62,5 MHz | Probado en `Lunar lader/` |
| ST7789: backlight **activo en bajo**; BLK fijo a GND funciona | Probado en `Lunar lader/` |
| Todos los módulos a **3V3**, desde una **fuente externa aparte** (no de la Pico) | Confirmado por el usuario |
| Posición intermedia del carrito: **por tiempo** (prototipo) | Confirmado por el usuario |
| **Tercer IR junto al TCS3200**: detecta presencia y distancia de la muestra ("acerque el objeto" / "no hay objeto") | Confirmado por el usuario |
| Uno de los 4 botones inicia la detección (B1) | Confirmado por el usuario |
| LED RGB de **ánodo común** (activo en bajo) | Confirmado por el usuario |
| Firmware en **C**, acceso a periféricos por `struct` de registros propios | Requisito del curso |

## 2. Pinout propuesto

> **Para cablear, usar `docs/conexiones.md`** (tabla por módulo, mapa por GPIO y diagrama del conector).
> Se genera con `python tools/generar_conexiones.py` a partir de `src/configuracion/pines.h`.

`P` = pin físico de la Pico W.

| Señal | GPIO | P | Función RP2040 | Notas |
|---|---|---|---|---|
| Botón 4 (guardar/menú) | GP0 | 1 | SIO entrada, pull-up | activo en bajo |
| IR proximidad OD (digital) | GP1 | 2 | SIO entrada | hay / no hay objeto |
| TCS3200 S2 | GP2 | 4 | SIO salida | selección de filtro |
| TCS3200 S3 | GP3 | 5 | SIO salida | selección de filtro |
| IR inicio | GP4 | 6 | SIO entrada + IRQ | fin de carrera izquierdo |
| TCS3200 OUT | GP5 | 7 | **PWM2 B, contador de flancos** | frecuencia medida por hardware |
| IR fin | GP6 | 9 | SIO entrada + IRQ | fin de carrera derecho |
| TB6612 STBY | GP7 | 10 | SIO salida | 0 = motor deshabilitado |
| TB6612 PWMA | GP8 | 11 | PWM4 A | velocidad (~20 kHz) |
| TB6612 AIN1 | GP9 | 12 | SIO salida | sentido |
| TB6612 AIN2 | GP10 | 14 | SIO salida | sentido |
| LED R (cátodo) | GP11 | 15 | PWM5 B, invertido | resistencia en serie |
| LED G (cátodo) | GP12 | 16 | PWM6 A, invertido | resistencia en serie |
| LED B (cátodo) | GP13 | 17 | PWM6 B, invertido | resistencia en serie |
| LED ánodo común | — | — | — | +3V3 de la fuente externa |
| Botón 1 (iniciar detección) | GP14 | 19 | SIO entrada, pull-up | activo en bajo |
| Buzzer | GP15 | 20 | PWM7 B | igual que Lunar Lander |
| ST7789 DC | GP16 | 21 | SIO salida | verificado |
| ST7789 RES | GP17 | 22 | SIO salida | verificado |
| ST7789 SCK | GP18 | 24 | SPI0 SCK | verificado |
| ST7789 SDA | GP19 | 25 | SPI0 TX | verificado |
| Botón 2 (calibrar) | GP20 | 26 | SIO entrada, pull-up | |
| Botón 3 (posicionar) | GP21 | 27 | SIO entrada, pull-up | |
| Joystick SW | GP22 | 29 | SIO entrada, pull-up | igual que Lunar Lander |
| Joystick JX | GP26 | 31 | ADC0 | |
| Joystick JY | GP27 | 32 | ADC1 | |
| IR proximidad OA (analógica) | GP28 | 34 | ADC2 | distancia relativa de la muestra |
| ST7789 BLK | — | — | — | cable fijo a GND |
| TCS3200 S0, S1 | — | — | — | cable fijo (ver §4) |
| TCS3200 OE | — | — | — | cable fijo a GND (salida siempre habilitada) |

Están asignados **los 26 GPIO usables**; no queda ninguno libre (la depuración va por USB).

### Uso de las slices PWM

| Slice | Uso | Por qué así |
|---|---|---|
| 2 | TCS3200 OUT (canal B, modo contador) | el contador ocupa la slice entera; GP4/GP20/GP21 de esa slice van como SIO |
| 4 | Motor PWMA (~20 kHz, fuera del rango audible) | frecuencia propia |
| 5, 6 | LED R / G / B | comparten frecuencia, pueden compartir slices |
| 7 | Buzzer | cambia TOP/DIV para cada nota → slice exclusiva |
| 0, 1, 3 | libres | |

### Comprobación de conflictos

`pines.h` falla en compilación si un GPIO se repite, si se usa
GP23/24/25/29, si el SPI0 o el ADC caen en pines que no los tienen, si el OUT
del TCS3200 no es impar o si dos funciones con frecuencias distintas comparten
slice. Comprobado: los cuatro tipos de error se detectan.

### Recursos del RP2040 reservados por el SDK/CYW43

- **PIO y DMA**: el driver del CYW43 reclama una máquina PIO y canales DMA de
  forma dinámica. Si el firmware usa PIO o DMA, debe reclamarlos por la API de
  *claim* del SDK para no pisarlos.
- **TIMER alarma 3**: la usa el *alarm pool* por defecto del SDK (`sleep_ms`,
  `async_context` del CYW43). Las alarmas propias usarán 0–2.
- **USBCTRL**: lo usa `stdio` por USB.

## 3. Alimentación (fuente externa de 3V3)

```
Fuente 3V3 externa ──┬── VCC de TCS3200, IR ×2, joystick, botones, ST7789, TB6612 (VCC)
                     │
                     └── (opcional) ──|>|── VSYS de la Pico (P39)   diodo Schottky
                                       │
GND fuente ──────────┴── GND de todos los módulos ── GND de la Pico (P3/P8/…, AGND P33)
```

Reglas:

1. **GND común obligatorio** entre fuente, módulos y Pico.
2. **No conectar** el +3V3 de la fuente al pin 36 de la Pico, 3V3(OUT): eso pone dos
   fuentes en paralelo.
3. La Pico se alimenta por **USB** o por **VSYS (P39) a través de un Schottky**
   (VSYS admite 1,8–5,5 V; el diodo evita retroalimentar al conectar USB).
4. **Secuencia de encendido**: encender la Pico y los módulos a la vez, o la Pico
   primero. Un módulo encendido con la Pico apagada empuja corriente por sus
   salidas hacia GPIO sin alimentar (TCS3200 OUT, salidas de los IR).
5. **Referencia del ADC**: el ADC de la Pico usa su propio 3V3 interno y el
   joystick usa el 3V3 externo. Las dos tensiones no serán idénticas, así que
   el centro y los extremos del joystick se calibran por software.
6. **Ruido del motor**: si el motor comparte esta fuente, poner un
   electrolítico grande (≥ 470 µF) junto al VM del TB6612 y 100 nF en cada
   módulo. Los picos del motor afectan la lectura del TCS3200 y el ADC.

## 4. Hipótesis (pendientes de confirmar en el banco)

| Hipótesis | Cómo verificarla |
|---|---|
| Los IR de fin de carrera dan salida digital **activa en bajo** (`FINAL_ACTIVO_BAJO`) | Prueba del motor, paso 0: mover el carrito a mano y mirar el USB |
| Con `MOTOR_SIGNO_HACIA_FIN = +1`, velocidad positiva mueve hacia el FINAL | Prueba del motor, paso 3: pulso corto con B2 |
| En los IR, **OA = salida analógica** y **OD = salida digital** (comparador) | Multímetro en cada pin acercando la mano: la analógica varía de forma continua, la digital salta entre 0 y 3V3 |
| La lectura analógica del IR de proximidad depende del **color de la muestra**, no solo de la distancia: una muestra negra refleja poco IR y parece "lejos" | Medir OA con papel blanco y negro a la misma distancia. Si difieren mucho, usar OA solo para "hay / no hay objeto" y "acerque", sin calibrar en mm |
| El LED IR del tercer sensor **contamina la lectura del TCS3200**: sus fotodiodos son sensibles al IR y el módulo no trae filtro de corte IR | Medir un mismo color con el IR conectado y desconectado. Si cambia, opciones: separar o apantallar el emisor, o apagarlo durante la medición con un transistor PNP/P-MOSFET en su VCC usando GP1 (y quedarse solo con la salida analógica) |
| El TCS3200 funciona a 3V3 (su rango típico de alimentación es 2,7–5,5 V) | Medir la frecuencia de OUT con la Pico |
| S0/S1 cableados fijos (p. ej. S0 = 3V3, S1 = GND → escala 20 %) bastan; no hace falta cambiar la escala por software | Revisar que la frecuencia máxima con blanco quepa en la ventana de conteo |
| Los IR funcionan a 3V3 y su salida digital no tiene pull-up a otra tensión | Medir la salida con multímetro, con y sin obstáculo |
| Ancho de banda de SPI a 62,5 MHz estable en el nuevo cableado | Si salen rayas, bajar a 31,25 MHz |

## 5. LED RGB (ánodo común)

- Ánodo al +3V3 **de la misma fuente externa**; cada cátodo pasa por su
  resistencia y llega a su GPIO. El GPIO **absorbe** la corriente (pin en 0 = encendido).
- Resistencias: a 3,3 V quedan ~1,3 V sobre el rojo y solo ~0,3–0,5 V sobre
  verde y azul (Vf ≈ 2,8–3,0 V). Valores orientativos: **R ≈ 150 Ω, G/B ≈ 47 Ω**
  (≈ 8 mA máx.); ajustar a ojo para balancear el blanco. Mantener ≤ 12 mA por pin.
  Verde y azul darán poco brillo a 3V3; es una limitación del LED, no del firmware.
- En PWM se activa `A_INV`/`B_INV` para que duty 100 % = brillo máximo.
- Al arrancar, los GPIO están en alta impedancia → LED apagado (correcto).

## 6. Decisiones pendientes

1. **Sensores IR**: cuál de OA / OD es la salida digital (la otra podría ser
   analógica; no queda ningún ADC libre para ella).
2. **Motor**: ¿VM sale de la misma fuente de 3V3? Un motor de 5 V a 3V3 gira
   más lento y con menos par. Hace falta la **corriente máxima de la fuente** frente
   a la corriente de arranque del motor.
3. **TCS3200 S0/S1**: confirmar que van fijos por cable (libera GP0/GP1).
4. **Buzzer**: un buzzer pasivo puede pedir más corriente de la que entrega un GPIO
   (12 mA máx.). Usar un transistor NPN como en Lunar Lander.
5. **Pico**: alimentada por USB o por VSYS desde la fuente externa.

## 7. Motor y finales de carrera (TB6612FNG)

Código: `drivers/motor.c`, `drivers/finales.c`, `aplicacion/carro.c`,
`dominio/limites.c`; parámetros en `src/configuracion/motor.h`.

### Conexión

| TB6612FNG | Conexión | Notas |
|---|---|---|
| VCC | +3V3 de la fuente externa | lógica |
| VM | **alimentación del motor (ver aviso)** | |
| GND | GND común | obligatoria |
| STBY | GP7 | 0 = puente apagado; el firmware arranca en 0 |
| PWMA | GP8 (PWM4 A, 20 kHz) | |
| AIN1 / AIN2 | GP9 / GP10 | sentido |
| AO1 / AO2 | motor | |
| BIN1, BIN2, PWMB | a GND | canal B sin usar |

> **AVISO — `VM` no puede salir del 3V3.** Según el datasheet del TB6612FNG
> (Toshiba) el rango de `VM` es de unos 4,5 a 13,5 V (**verificarlo en tu hoja de
> datos**). Con 3,3 V el puente no conduce bien y el motor de 5 V casi no gira.
> Hace falta una fuente aparte de 5 V para `VM` (con la corriente de arranque
> del motor, no solo la nominal), con **GND unida** al resto. Poner un
> electrolítico de ≥ 470 µF junto a `VM`/`GND` del TB6612.

### Decisiones de seguridad (todas en el código)

1. **Arranque desarmado:** `STBY = 0` y entradas a 0 antes de configurar el PWM.
2. **Fallo seguro de los finales:** el pull interno apunta al estado *activo*:
   un cable suelto se lee como "extremo alcanzado" y el carrito no se mueve.
3. **Con un extremo activo solo se puede alejarse de él;** con los dos
   activos a la vez no hay movimiento y se declara error.
4. **Freno inmediato** (IN1 = IN2 = 1) al tocar un extremo; el resto de las
   paradas usan rampa.
5. **Tiempo máximo continuo** `CARRO_TIMEOUT_MS`: si se excede, freno y error
   hasta reiniciarlo (B4 largo). Sin encoder, es la única protección contra un
   sensor que no se active.
6. El sentido eléctrico (`MOTOR_SIGNO_HACIA_FIN`) y la polaridad de los IR
   (`FINAL_ACTIVO_BAJO`) son **hipótesis**: el programa de prueba las verifica
   antes de mover nada.

### Homing, calibración y posición por tiempo

Código: `aplicacion/eje.c`, `dominio/posicion.c`; parámetros en `configuracion/eje.h`.

- **Ciclo de referencia (B2 en la prueba):** busca el INICIO despacio → fija 0 →
  recorre hasta el FINAL midiendo el tiempo y la velocidad aplicada → vuelve al INICIO.
- **Modelo:** `posición = ∫velocidad·dt / ∫total × 350 mm`. No hace falta conocer los
  mm/s; la rampa y la zona muerta del motor quedan incluidas en la medición.
- **Resincronización:** cada vez que el carrito toca un final se reajusta la posición,
  así que el error no se acumula entre viajes. Los destinos 0 y 350 mm se alcanzan
  *hasta el final de carrera real*, no por estimación.
- **Exactitud (código real ejecutado en el PC, `test/host/`):** un motor ideal da
  ≤ 1,6 mm. Con una variación de velocidad de ±3 % en cada viaje (tensión,
  correa, temperatura) el error crece con la distancia recorrida: sin precauciones
  llegó a ±11 mm y 17-23 mm de dispersión entre viajes. Dos decisiones lo corrigen:
  1. **Misma velocidad en la aproximación que en el viaje y la calibración**
     (`EJE_VEL_APROX = EJE_VEL_VIAJE`): el motor real no es proporcional al duty,
     y a otra velocidad el modelo falla 9-19 mm de forma sistemática.
  2. **Referencia previa** (`EJE_REFERENCIA_PREVIA`): para ir a un punto intermedio
     se pasa antes por el final de carrera más cercano, donde la posición se
     resincroniza sin error, y se recorre solo el último tramo (≤ 175 mm).
  Resultado (±3 %): error típico 1-4 mm (peor caso entre 6 semillas: 6,6 mm) y
  repetibilidad de 2-3 mm, a cambio de ≈ 1,2 s más por movimiento.
  **El modelo del motor es una hipótesis**: no incluye inercia al frenar, holgura
  de la correa ni deslizamiento. Medir con regla (paseo de la prueba) y, si el
  carrito sigue avanzando tras frenar, poner esa distancia en `EJE_DIST_FRENO_UM`.
- **Si hace falta más exactitud:** el posicionamiento por tiempo no baja de unos
  pocos mm. Opciones de hardware: marcas negras impresas junto a la escala leídas
  por un IR reflectivo en el carrito (cuenta marcas = lazo cerrado), un encoder
  en el eje del motor, o un motor paso a paso.

### Riesgo: sin encoder, un final de carrera estropeado empuja contra el tope

La única protección es el tiempo máximo, derivado de `EJE_RECORRIDO_ESPERADO_MS`
(8 s por defecto, **hipótesis: medirlo**). En simulación, con un final estropeado el
motor empuja contra el tope mecánico ≈ 3 s (calibración) o hasta ≈ 11 s (homing desde
el medio, si falla el INICIO). Para reducirlo: ajustar bien ese valor, bajar
`EJE_VEL_HOMING`, o dejar que la correa patine contra el tope (embrague/fricción).
Hay que conocer la **corriente de bloqueo del motor** frente a los 1,2 A continuos del
TB6612FNG (valor del datasheet de Toshiba: verificarlo).

## 8. Del color a la posición (escala impresa)

Código: `dominio/escala.c`, `aplicacion/deteccion.c`; parámetros en `configuracion/escala.h`.

- **Mapeo:** `X = desplazamiento + (H − H0)/360 × ancho`, con H del color ya
  calibrado (negro/blanco) y promediado sobre 3 mediciones.
- **Rojo en el cierre:** el 0° y el 360° son el mismo color y ambos extremos están
  impresos en rojo. Con un tono dentro de ±10° del rojo el cursor va al extremo más
  cercano a donde ya está (sin esto, el ruido 358° ↔ 2° produce saltos de ≈350 mm;
  en simulación: 193 saltos largos sin la política, ninguno con ella).
- **Acromáticos:** V < 40 → negro; S < 45 → blanco (V ≥ 180) o gris. No tienen tono,
  así que por defecto el cursor NO se mueve (`ESCALA_ACROMATICO_AL_INICIO`).
  Los colores pastel con S cercana a 45 (beige, rosa pálido) quedan como cromáticos
  pero con tono poco fiable.
- **Zona muerta:** no se vuelve a mover si el cursor ya está a menos de 3 mm. Un
  ruido de tono de ±5° son ±4,9 mm: ajustar con la estabilidad real del sensor.
- **Medir solo con el carrito parado:** medir bloquea ≈ 270 ms sin vigilar los finales
  de carrera (`eje_quieto()` lo impone).

### Medidas físicas que hay que tomar con regla

| Parámetro | Qué es | Valor actual |
|---|---|---|
| `EJE_LONGITUD_UM` | recorrido REAL entre los dos finales de carrera | 320 mm (hipótesis) |
| `ESCALA_ANCHO_UM` | ancho de la escala impresa | 320 mm (hipótesis) |
| `ESCALA_DESPLAZAMIENTO_UM` | del final INICIO al borde izquierdo impreso | 0 (hipótesis) |
| `ESCALA_TONO_INICIAL_GRADOS` | tono impreso en el borde izquierdo | 0° = rojo |

El build falla si `desplazamiento + ancho > recorrido`. La escala impresa con la
DeskJet debe salir al 100 % y verificarse con regla; los colores impresos no
coincidirán exactamente con los RGB digitales (tinta, papel, iluminación): conviene
calibrar el mapeo con muestras impresas de la propia escala.

## 9. Interfaz gráfica (ST7789 240×240)

Código: `graficos/` (fuente y texto), `ui/interfaz.c`, `dominio/historial.c`;
integración en `pruebas/prueba_interfaz.c`.

- **Fuente 5×7** generada por `tools/generar_fuente.py` (ASCII + `á é í ó ú ñ ü ¿ ¡ °`;
  sin mayúsculas acentuadas ni `Ñ`). Las cadenas son UTF-8 normales en el código.
  El script dibuja los glifos en ASCII para revisarlos y comprueba los acentos.
- **Pantallas:** principal (color, HEX, R/G/B, H/S/V, posición y objetivo, escala con
  marcas, estado, sensores IR y chips CAL/REF/ARM/WiFi), menú de 8 opciones,
  historial de 16 lecturas, mover cursor e información.
- **Controles:** B1 medir · B2 calibración · B3 mover cursor · B4 corto guardar /
  largo o SW menú · joystick arriba/abajo navega y X ajusta los mm.
- **Diseño:** la interfaz recibe una copia del estado (`ui_modelo_t`) y devuelve
  órdenes (`ui_orden_t`); no toca sensor ni motor. `ui_dibujar()` pinta **una sección
  por llamada** (≤ ≈ 5 ms) para no dejar de vigilar el motor; cambiar de pantalla
  borra todo de una vez (≈ 15 ms), conviene hacerlo con el carrito parado.
- **Verificación:** las 10 pantallas se renderizan en el PC (`test/host/`) y se
  revisaron como imagen: eso encontró restos de texto entre el estado "sin lectura"
  y el normal, y un pie de menú cortado. Falta verlas en la pantalla real.
- **Pendiente:** el historial vive en RAM (se pierde al reiniciar); el Wi-Fi aparece
  como "reservado".

## 10. Máquina de estados e IR de proximidad

Código: `aplicacion/sistema.c` (máquina de estados), `aplicacion/app.c` (aplicación),
`dominio/proximidad.c` y `drivers/proximidad.c` (IR); parámetros en
`configuracion/sistema.h` y `configuracion/proximidad.h`.

### Estados

```
INIT → IDLE ⇄ HOMING            referenciar el eje (busca inicio, calibra, vuelve)
            ⇄ MEASURE           espera el objeto, mide, apunta el cursor
            ⇄ CALIBRATE         espera el objeto, calibra negro o blanco
            ⇄ MOVE_TO_TARGET    el cursor va a una posición
            ⇄ MANUAL            movimiento con el joystick
    cualquiera → ERROR → (B4 largo) → IDLE
```

`sistema` no toca pantalla, botones, buzzer ni LED: recibe órdenes, devuelve un
motivo de rechazo si no las acepta y emite eventos (que `app.c` convierte en sonido,
LED y mensajes). Por eso se prueba con el código real en el PC.

- **Reglas:** solo se acepta una operación a la vez (el resto se rechaza con "ocupado");
  medir exige el color calibrado; mover y referenciar exigen el motor armado; en ERROR todo
  se rechaza hasta reiniciarlo. Desarmar el motor cancela lo que esté en curso.
- **Cancelar:** B4 o SW durante HOMING, MEASURE, CALIBRATE o MOVE_TO_TARGET.
- **Referenciar tarda ≈ 20 s** (busca el inicio, recorre los 350 mm midiendo y vuelve).

### Medir y calibrar esperan al objeto

La distancia cambia la intensidad medida, así que ni se mide ni se calibra el color hasta
que el IR vea el objeto **a la distancia correcta de forma continua durante 300 ms**.
Mientras tanto la pantalla guía al usuario ("No hay objeto", "Acerca el objeto",
"Aleja un poco el objeto") y suena una alerta en cada cambio. Pasados 8 s sin
conseguirlo se cancela. El IR clasifica en cuatro estados con histéresis y un filtro de
6 lecturas consecutivas: `nada` · `lejos` · `ok` · `cerca`.

### IR de proximidad: hay que medirlo en el banco

Los valores por defecto son **hipótesis** (módulo tipo TCRT5000: OD activa en bajo, OA baja
al acercar). Cargar `PRUEBA_PROXIMIDAD` en `main.c`: el programa mide con tres tomas
(sin objeto, distancia correcta, demasiado cerca) y **imprime las líneas exactas** que hay
que poner en `configuracion/proximidad.h`, incluidas las dos polaridades.

- La lectura depende del **color de la muestra**: el negro refleja poco IR y parece más
  lejos. Repetir con una muestra clara y una oscura y elegir umbrales que sirvan para las dos.
- **Riesgo ya señalado (§4):** el LED infrarrojo del módulo puede alterar la medida del
  TCS3200, que no lleva filtro de corte IR. Medir la misma muestra con el IR conectado y
  desconectado.
- Si el sensor no está conectado, `PROX_HABILITADO 0` hace que medir y calibrar no esperen.
- Con el cable de OD suelto se lee "sin objeto": el sistema no medirá nunca y la guía
  dirá "No hay objeto" (fallo visible, no peligroso).

### Qué cargar

`PRUEBA_ACTIVA` en `src/main.c`: `MODO_APLICACION` es la aplicación completa; las
`PRUEBA_*` son los programas aislados de cada etapa (pantalla, TCS3200, entradas,
actuadores, motor, eje, mapeo, proximidad).

## 11. Pruebas en el PC

`test/host/compilar.sh` compila el código real de la interfaz y del eje contra una
pantalla, un motor y unos finales simulados (ver `test/host/LEEME.md`). Requiere
`D:\dev\w64devkit` y, para los montajes PNG, Python con Pillow.

## 12. Mejoras para versiones futuras

### v1.2 — Envío de mediciones por Bluetooth a un HC-06

Pospuesto a propósito; **no forma parte de la v1.0**.

- Esquema: Pico W (maestro SPP, Bluetooth Classic) → HC-06 (esclavo) → USB-TTL → PC.
- No consume GPIO: la radio va en el CYW43. Requiere `pico_cyw43_arch`,
  `pico_btstack_classic` y un `btstack_config.h`; referencia en el SDK:
  `lib/btstack/example/spp_streamer_client.c`.
- La pila BT es código del SDK/BTstack (misma excepción que el Wi-Fi).
- Formato propuesto, una línea por medición:
  `MED;R=212;G=48;B=35;HEX=#D43023;H=7;X=6.8mm;PROX=OK`
- Datos por confirmar en su momento: MAC o nombre del HC-06 y su PIN (por defecto `1234`).
- Alternativa: Pico W como esclavo SPP, emparejada directo con el PC (sin HC-06).
