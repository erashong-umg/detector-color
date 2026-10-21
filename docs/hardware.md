# Hardware — Detector de color con carrito lineal (cálido / frío / neutro)

Estado al 2026-10-20. Las asignaciones de pines viven en código en
`src/configuracion/pines.h`; si este documento y ese archivo difieren, manda
el archivo (tiene validación en compilación).

## 1. Hechos verificados

| Hecho | Fuente |
|---|---|
| Placa: **Raspberry Pi Pico W** (RP2040 + CYW43439) | Confirmado por el usuario |
| GP23 (WL_ON), GP24 (WL_D), GP25 (WL_CS), GP29 (WL_CLK / ADC3 = VSYS/3) los usa el chip Wi-Fi | Datasheet Pico W |
| ADC disponible en la Pico W: solo **GP26, GP27, GP28** (AIN0–2) | Datasheet Pico W |
| ST7789 = módulo de 2" **240×320 (GMT020-02, ST7789V) con CS y BL fijos (CS a GND, BL a 3V3)**, 7 pines GND/VCC/SCL/SDA/RES/DC/BLK | Pinout del módulo real; el cableado base viene de Lunar Lander (módulo sin CS) |
| ST7789: SPI **modo 3**, **INVON**, MADCTL `0x00`, sin offsets, 62,5 MHz | Probado en `Lunar lader/` |
| ST7789: CS fijo a GND; BL fijo a 3V3 (activo en alto, HIPÓTESIS). En Lunar Lander el módulo sin CS usaba BLK a GND | **Sin probar** con este módulo |
| Todos los módulos a **3V3**, desde una **fuente externa aparte** (no de la Pico) | Confirmado por el usuario |
| Posición intermedia del carrito: **por tiempo** (prototipo) | Confirmado por el usuario |
| **Tercer IR junto al TCS3200**: detecta presencia y distancia de la muestra ("acerque el objeto" / "no hay objeto") | Confirmado por el usuario |
| Uno de los 4 botones inicia la detección (B1) | Confirmado por el usuario |
| Firmware en **C**, acceso a periféricos por `struct` de registros propios | Requisito del curso |

### Medido en el banco (2026-10-20)

| Qué | Resultado | Dónde queda |
|---|---|---|
| Finales IR (INICIO GP4, FIN GP6) | salida **activa en bajo** (nivel 0 = activo); los dos frenan el motor en su extremo | `FINAL_ACTIVO_BAJO 1` |
| Sentido del motor | con velocidad positiva (B2, AIN1 alto) el carrito va hacia el **FIN** | `MOTOR_SIGNO_HACIA_FIN +1` |
| Botones B1–B4 | módulo **activo en ALTO** (resistencia a GND y el botón lleva el pin a 3V3): con la lógica en bajo se leían los cuatro pulsados; el SW del joystick es activo en bajo | `BOTONES_ACTIVO_ALTO 1` en `entradas.h` |
| VM del TB6612 | **5 V** de una fuente aparte, con GND común | — |
| Potencia mínima del motor con 5 V | 30 %: apenas se mueve · 60 %: 2–4 mm · 80 %: a trompicones · 90 %: mejor · 100 %: con fuerza | `MOTOR_DUTY_MIN_PCT 90` |
| Recorrido completo INICIO→FIN | **320 mm** medidos con regla entre las posiciones de INICIO y FIN, y **≈ 0,74–0,79 s** a `EJE_VEL_VIAJE` (duty ≈ 94 %, ≈ 410 mm/s); el comando `ESTADO` lo da en `cal_ms` | `EJE_RECORRIDO_ESPERADO_MS 1000`, `EJE_RECORRIDO_MIN_MS 200`, `CARRO_TIMEOUT_MS 2500` |
| TCS3200 con el pin LED a 3V3 | sin muestra ≈ 800–3500 Hz (cambia con la luz del cuarto); negro ≈ 1000 / 1100 / 1400 Hz (R/G/B) y C ≈ 3500; blanco ≈ 9000–11000 Hz y C ≈ 29 000. Recortes posibles en B (≈ 12 kHz) con muestras muy claras | `tcs3200.h` |
| Detección de color (calibrado negro y blanco) | rojo H = 0°, verde H ≈ 105–112°, azul claro H ≈ 217°; un azul muy oscuro (V ≈ 4) no es fiable | — |
| IR de proximidad | **OD activa en bajo**; OA **se satura**: sin objeto ≈ 4000, con papel blanco 70–270 a cualquier distancia dentro del alcance (no distingue «cerca» de «ok»); **no ve el negro** (casi tocando, ≈ 4000) | `PROX_NIVEL_CERCA 3990` (estado «cerca» inalcanzable) y B1 mantenido fuerza la medición |
| Pantalla ST7789 | 240×320 sin offsets; el ruido de abajo era la RAM sin escribir | `pantalla.h` |

Pendiente de medir: precisión de las tres posiciones con regla (en curso: las paradas ideales son 53,3 / 160,0 /
266,7 mm desde el INICIO; la primera parada en 160 mm quedó, según el firmware, en 159,5 mm), arranque pegado al
FIN y al INICIO, y el valor real de S0/S1 del TCS3200 (el cambio de cableado no alteró las frecuencias).

## 2. Pinout propuesto

> **Para cablear, usar `docs/conexiones.md`** (tabla por módulo, mapa por GPIO y diagrama del conector).
> Se genera con `python tools/generar_conexiones.py` a partir de `src/configuracion/pines.h`.

`P` = pin físico de la Pico W.

| Señal | GPIO | P | Función RP2040 | Notas |
|---|---|---|---|---|
| Botón 4 (guardar/menú) | GP0 | 1 | SIO entrada, pull-down | activo en **alto** |
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
| Botón 1 (medir) | GP14 | 19 | SIO entrada, pull-down | activo en **alto** |
| Buzzer | GP15 | 20 | PWM7 B | igual que Lunar Lander |
| ST7789 DC | GP16 | 21 | SIO salida | verificado |
| ST7789 RES | GP17 | 22 | SIO salida | verificado |
| ST7789 SCK | GP18 | 24 | SPI0 SCK | verificado |
| ST7789 SDA | GP19 | 25 | SPI0 TX | verificado |
| Botón 2 (calibrar / menú) | GP20 | 26 | SIO entrada, pull-down | activo en **alto** |
| Botón 3 (bajar en el menú) | GP21 | 27 | SIO entrada, pull-down | activo en **alto** |
| Joystick SW | GP22 | 29 | SIO entrada, pull-up | activo en bajo; hace lo mismo que B1 |
| Joystick JX | GP26 | 31 | ADC0 | |
| Joystick JY | GP27 | 32 | ADC1 | |
| IR proximidad OA (analógica) | GP28 | 34 | ADC2 | distancia relativa de la muestra |
| ST7789 CS | — | — | — | cable fijo a GND |
| ST7789 BL | — | — | — | cable fijo a 3V3 (si no se ve nada, probar GND) |
| TCS3200 S0, S1 | — | — | — | cable fijo (ver §4) |
| TCS3200 LED | — | — | — | cable fijo a 3V3 (enciende los LED blancos del módulo) |

Están asignados **23 de los 26 GPIO usables**; quedan libres **GP11, GP12 y GP13** (antes el LED RGB). La depuración va por USB.

### Uso de las slices PWM

| Slice | Uso | Por qué así |
|---|---|---|
| 2 | TCS3200 OUT (canal B, modo contador) | el contador ocupa la slice entera; GP4/GP20/GP21 de esa slice van como SIO |
| 4 | Motor PWMA (~20 kHz, fuera del rango audible) | frecuencia propia |
| 7 | Buzzer | cambia TOP/DIV para cada nota → slice exclusiva |
| 0, 1, 3, 5, 6 | libres | |

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

## 4. Hipótesis iniciales (varias ya resueltas: ver «Medido en el banco» en §1)

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

## 5. LED RGB (retirado)

El LED RGB se eliminó del proyecto: GP11, GP12 y GP13 quedan libres y el firmware ya
no lo maneja. La realimentación al usuario es la pantalla y el buzzer.

## 6. Decisiones pendientes

1. **Sensores IR** — *resuelta:* OA es la analógica y OD la digital (activa en bajo); OA se satura
   con cualquier muestra clara y no ve las oscuras (ver §1).
2. **Motor** — *resuelta en parte:* VM va a una fuente aparte de **5 V**. Con ella el motor solo se mueve
   bien por encima del 90 % de ciclo de trabajo, así que no hay control fino de velocidad. **Pendiente:**
   subir VM (el TB6612 admite hasta ≈ 13,5 V) para poder ir más lento y bajar `MOTOR_DUTY_MIN_PCT`; medir la
   corriente máxima de la fuente frente a la de arranque del motor.
3. **TCS3200 S0/S1** — van fijos por cable (libera GPIO). Falta confirmar la escala real (2 % o 20 %).
4. **Buzzer**: un buzzer pasivo puede pedir más corriente de la que entrega un GPIO
   (12 mA máx.). Usar un transistor NPN como en Lunar Lander.
5. **Pico**: alimentada por USB o por VSYS desde la fuente externa.
6. **Serie físico (UART0 en GP0/GP1)** — descartado por ahora. Si se retoma, hay que mover B4 y OD del IR de
   proximidad a GP11–GP13.

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

> **Medido:** en el banco `VM` va a **5 V** de una fuente aparte; con eso el motor necesita ≥ 90 % de ciclo
> de trabajo para moverse bien (`MOTOR_DUTY_MIN_PCT`). Subir `VM` permitiría ir más lento.
>
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
5. **Tiempo máximo continuo** `CARRO_TIMEOUT_MS` (2,5 s): si se excede, freno y error
   hasta reiniciarlo (B4 largo o el comando `REINICIAR`). Sin encoder, es la única protección contra un
   sensor que no se active.
6. El sentido eléctrico (`MOTOR_SIGNO_HACIA_FIN`) y la polaridad de los IR
   (`FINAL_ACTIVO_BAJO`) están **verificados en el banco** (§1).
7. **Arrancar pegado al FIN:** si buscando el inicio el FIN no se suelta en `EJE_FIN_LIBERAR_MS` (400 ms), se
   declara sentido invertido o atasco en vez de empujar contra el tope.
8. **Recuperación automática:** un final desconectado se lee como activo; con los dos activos a la vez el
   sistema se detiene en ERROR, y cuando vuelven a ser coherentes durante `SISTEMA_RECUPERAR_MS` (0,5 s) el error se
   reinicia solo y se vuelve a referenciar el eje.
9. **Antirrebote de los finales:** `FINAL_ANTIREBOTE_MS` = 2 ms; el carrito recorre ≈ 0,4 mm por ms.

### Homing, calibración y posición por tiempo

Código: `aplicacion/eje.c`, `dominio/posicion.c`; parámetros en `configuracion/eje.h`.

- **Referencia automática y reintentos:** al encender, 2 s después de arrancar el sistema arma el motor y
  referencia solo (`SISTEMA_AUTOREFERENCIAR`); si falla por algo recuperable (tiempo máximo o calibración
  demasiado corta) lo reintenta hasta 2 veces más. Funciona desde cualquier posición del carrito (el
  evento `AUTOREFERENCIANDO` indica si salió de INICIO, FIN o MEDIO) y no reintenta con finales
  incoherentes ni con el sentido invertido.
- **Ciclo de referencia (B2 en la prueba):** busca el INICIO despacio → fija 0 →
  recorre hasta el FINAL midiendo el tiempo y la velocidad aplicada → vuelve al INICIO.
- **Modelo:** `posición = ∫velocidad·dt / ∫total × 320 mm`. No hace falta conocer los
  mm/s; la rampa y la zona muerta del motor quedan incluidas en la medición.
- **Resincronización:** cada vez que el carrito toca un final se reajusta la posición,
  así que el error no se acumula entre viajes. Los destinos 0 y 320 mm se alcanzan
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
  **Con el recorrido real de ≈ 0,75 s la simulación empeora** (errores de hasta ≈ 30 mm): a esa
  velocidad la rampa y la reacción de los finales pesan mucho. Subir VM para ir más lento es la mejora de
  fondo.
  Resultado simulado con un recorrido de 8 s (±3 %): error típico 1-4 mm (peor caso entre 6 semillas: 6,6 mm) y
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
(1 s, medido: el recorrido completo tarda ≈ 0,75 s). En simulación, con un final estropeado el
motor empuja contra el tope mecánico ≈ 2 s (homing o calibración). Para reducirlo: ajustar bien ese valor, bajar
`EJE_VEL_HOMING`, o dejar que la correa patine contra el tope (embrague/fricción).
Hay que conocer la **corriente de bloqueo del motor** frente a los 1,2 A continuos del
TB6612FNG (valor del datasheet de Toshiba: verificarlo).

## 8. Del color a la posición (escala impresa)

Código: `dominio/escala.c`, `aplicacion/deteccion.c`; parámetros en `configuracion/escala.h`.

- **Clasificación:** el color ya calibrado (negro/blanco) y promediado sobre 3 mediciones
  se convierte a HSV y se clasifica en **cálido**, **frío** o **neutro**:
  - cálido: tono de 315° a 360° y de 0° a 75° (rojo, naranja, amarillo, magenta rojizo);
  - frío: tono de 75° a 315° (verde, cian, azul, violeta);
  - neutro: colores sin tono (ver acromáticos).
  Los cortes (`ESCALA_TONO_FRIO_DESDE` = 75°, `ESCALA_TONO_CALIDO_DESDE` = 315°) son una
  hipótesis a ajustar con muestras reales.
- **Posiciones fijas:** el carrito (poleas y correa sobre el eje X, motor DC con TB6612FNG)
  va a una de tres posiciones: cálido a 1/6, neutro a 1/2 y frío a 5/6 del ancho de
  trabajo (`ESCALA_POS_CALIDO_UM`, `ESCALA_POS_NEUTRO_UM`, `ESCALA_POS_FRIO_UM`). Es la
  parada en el centro de cada tercio.
- **Acromáticos:** V < 40 → negro; S < 45 → blanco (V ≥ 180) o gris. No tienen tono,
  así que se consideran **neutros** y el carrito va a la posición central.
  Los colores pastel con S cercana a 45 (beige, rosa pálido) quedan como cromáticos
  pero con tono poco fiable. Un tono cerca de un corte (75° o 315°) puede cambiar de
  temperatura por el ruido de la medida.
- **Zona muerta:** no se vuelve a mover si el cursor ya está a menos de 3 mm. Un
  ruido de tono de ±5° son ±4,9 mm: ajustar con la estabilidad real del sensor.
- **Medir solo con el carrito parado:** medir bloquea ≈ 270 ms sin vigilar los finales
  de carrera (`eje_quieto()` lo impone).

### Medidas físicas que hay que tomar con regla

| Parámetro | Qué es | Valor actual |
|---|---|---|
| `EJE_LONGITUD_UM` | recorrido REAL entre los dos finales de carrera | 320 mm (**medido con regla**) |
| `ESCALA_ANCHO_UM` | ancho de la zona de trabajo (se divide en tres tercios) | 320 mm (= todo el recorrido) |
| `ESCALA_DESPLAZAMIENTO_UM` | del final INICIO al borde izquierdo de la zona de trabajo | 0 (la zona empieza en el final INICIO) |
| `ESCALA_TONO_FRIO_DESDE` / `ESCALA_TONO_CALIDO_DESDE` | cortes de tono cálido / frío | 75° / 315° (hipótesis) |

El build falla si `desplazamiento + ancho > recorrido`. Conviene calibrar los cortes de
tono con muestras reales: los colores impresos no coinciden exactamente con los RGB
digitales (tinta, papel, iluminación).

## 9. Interfaz gráfica (ST7789 240×320)

Código: `graficos/` (fuente y texto), `ui/interfaz.c`, `dominio/historial.c`;
integración en `pruebas/prueba_interfaz.c`.

- **Fuente 5×7** generada por `tools/generar_fuente.py` (ASCII + `á é í ó ú ñ ü ¿ ¡ °`;
  sin mayúsculas acentuadas ni `Ñ`). Las cadenas son UTF-8 normales en el código.
  El script dibuja los glifos en ASCII para revisarlos y comprueba los acentos.
- **Pantallas:** principal (color, HEX, R/G/B, H/S/V, posición y objetivo, escala de tres zonas
  (cálido / neutro / frío) con marcas, barra cromática de 0° a 360° con la marca del tono medido, estado, sensores IR y chips CAL/REF/ARM/WiFi), menú de 8 opciones,
  historial de 16 lecturas, mover cursor e información.
- **Controles:** en la principal B1 (o SW) mide, B2 abre el menú de calibración, B3 no hace nada y
  B4 mantenido guarda la lectura y B4 corto abre el menú. En el menú, B2/B3 o el joystick navegan, B1 confirma y B4 sale;
  «Mover cursor» se abre desde el menú y en él el joystick X ajusta los mm.
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
            ⇄ MEASURE           espera el objeto, mide y lleva el carrito a su temperatura
            ⇄ CALIBRATE         espera el objeto, calibra negro o blanco
            ⇄ MOVE_TO_TARGET    el carrito va a una posición
            ⇄ MANUAL            selector de color: el joystick mueve el tono y el carrito va a su temperatura
    cualquiera → ERROR → (B4 largo o REINICIAR) → IDLE
    ERROR por finales incoherentes → (finales coherentes 0,5 s) → IDLE + referencia automática
```

`sistema` no toca pantalla, botones ni buzzer: recibe órdenes, devuelve un
motivo de rechazo si no las acepta y emite eventos (que `app.c` convierte en sonido y mensajes y
`protocolo.c` en líneas `EVT,…` por el puerto serie). Por eso se prueba con el código real en el PC.

- **Reglas:** solo se acepta una operación a la vez (el resto se rechaza con "ocupado");
  medir exige el color calibrado; mover y referenciar exigen el motor armado; en ERROR todo
  se rechaza hasta reiniciarlo. Desarmar el motor cancela lo que esté en curso.
- **Cancelar:** B4 durante HOMING, MEASURE, CALIBRATE o MOVE_TO_TARGET.
- **Referenciar tarda ≈ 2–3 s** (busca el inicio, recorre el eje midiendo y vuelve).
- **Muestras oscuras:** el IR de proximidad no las ve, así que **B1/SW mantenido** (o `FORZAR` por serie)
  mide o calibra sin esperarle.
- **Medir se avisa antes** (`SIS_EVT_ANALIZANDO`, una vuelta antes de bloquear ≈ 270 ms) para que el sonido de
  escáner ya esté sonando; durante la medición el buzzer sigue vivo por un gancho en la espera del sensor.

### Medir y calibrar esperan al objeto

La distancia cambia la intensidad medida, así que ni se mide ni se calibra el color hasta
que el IR vea el objeto **a la distancia correcta de forma continua durante 300 ms**.
Mientras tanto la pantalla guía al usuario ("No hay objeto", "Acerca el objeto",
"Aleja un poco el objeto") y suena una alerta en cada cambio. Pasados 8 s sin
conseguirlo se cancela. El IR clasifica en cuatro estados con histéresis y un filtro de
6 lecturas consecutivas: `nada` · `lejos` · `ok` · `cerca`.

### IR de proximidad: hay que medirlo en el banco

**Medido (§1):** OD activa en bajo; OA baja al acercar pero **se satura** (70–270 con cualquier muestra
clara dentro del alcance) y **no ve el negro**. El estado «cerca» queda desactivado
(`PROX_NIVEL_CERCA 3990`). Para volver a medirlo, cargar `PRUEBA_PROXIMIDAD` en `main.c`: el programa mide con tres tomas
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

`test/host/compilar.sh` compila el código real de la interfaz, del eje y del sistema contra una
pantalla, un motor, unos finales y un sensor de color simulados (ver `test/host/LEEME.md`): pantallas
renderizadas a PNG, simulación del eje, **autoreferenciar** (reintentos, recuperación al reconectar los
finales, sensor de color sin señal), **protocolo serie** y la **máquina de estados** completa. Requiere
`D:\dev\w64devkit` y, para los montajes PNG, Python con Pillow.

## 12. Mejoras para versiones futuras

### Wi-Fi

El CYW43 solo se maneja de forma realista con `pico_cyw43_arch` y lwIP, que chocan con la regla de «sin HAL del
SDK»: habría que consultarlo con el profesor y aislarlo en un único módulo. Ideas: punto de acceso con una página
de estado (color, temperatura, posición, historial) y botones de medir y calibrar.

### Puerto serie

El protocolo por USB ya existe (`aplicacion/protocolo.c`, ver el README). Un UART físico (por ejemplo para un
módulo Bluetooth HC-05) se haría por registros y exigiría liberar GP0/GP1 (B4 y OD del IR de proximidad), que
pueden pasar a GP11–GP13.

### Herramienta de diagnóstico aparte

`../prueba-motor-pantalla/` (fuera de este repositorio) es un programa independiente con el mismo pin-out para
probar el TB6612 y la pantalla con el multímetro: B1 conecta STBY, B2/B3 mueven el motor mientras se mantienen y
B4 cambia el modo de PWMA (PWM 30–100 % o GPIO fijo en alto).

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
