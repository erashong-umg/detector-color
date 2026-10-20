# Tabla de conexiones — Raspberry Pi Pico W ↔ sensores y periféricos

> **GENERADO** por `tools/generar_conexiones.py` a partir de `src/configuracion/pines.h`. No editar a mano:
> cambia `pines.h` y vuelve a ejecutar el script. Los números de pin físico son los del conector de
> 40 pines de la Pico / Pico W y se han cruzado con la tabla verificada de Lunar Lander.
>
> **Estado:** cableado propuesto, **sin comprobar con multímetro ni con el hardware**. Marcado
> «HIPÓTESIS» o «PENDIENTE» lo que depende de datos que aún no se han medido.

## Resumen: todas las conexiones en una tabla

| Módulo | Pin del módulo | Se conecta a | Pin físico de la Pico |
|---|---|---|:---:|
| **Pantalla ST7789 2" 230×240 (con CS y BL)** | GND | GND común | — |
|  | VCC | 3V3 de la fuente externa | — |
|  | SCL | GP18 (pin 24) | 24 |
|  | SDA | GP19 (pin 25) | 25 |
|  | RES | GP17 (pin 22) | 22 |
|  | DC | GP16 (pin 21) | 21 |
|  | CS | GND (fijo) | — |
|  | BL | 3V3 (fijo) | — |
| **Sensor de color TCS3200** | VCC | 3V3 de la fuente externa | — |
|  | GND | GND común | — |
|  | S0 | 3V3 (fijo) | — |
|  | S1 | GND (fijo) | — |
|  | LED | 3V3 (fijo) | — |
|  | S2 | GP2 (pin 4) | 4 |
|  | S3 | GP3 (pin 5) | 5 |
|  | OUT | GP5 (pin 7) | 7 |
| **IR final de carrera INICIO** | VCC | 3V3 de la fuente externa | — |
|  | GND | GND común | — |
|  | OD | GP4 (pin 6) | 6 |
|  | OA | sin conectar | — |
| **IR final de carrera FIN** | VCC | 3V3 de la fuente externa | — |
|  | GND | GND común | — |
|  | OD | GP6 (pin 9) | 9 |
|  | OA | sin conectar | — |
| **IR de proximidad (junto al TCS3200)** | VCC | 3V3 de la fuente externa | — |
|  | GND | GND común | — |
|  | OA | GP28 (pin 34) | 34 |
|  | OD | GP1 (pin 2) | 2 |
| **Driver de motor TB6612FNG + motor DC** | VM | alimentación del MOTOR (fuente aparte) | — |
|  | VCC | 3V3 de la fuente externa | — |
|  | GND | GND común | — |
|  | STBY | GP7 (pin 10) | 10 |
|  | PWMA | GP8 (pin 11) | 11 |
|  | AIN1 | GP9 (pin 12) | 12 |
|  | AIN2 | GP10 (pin 14) | 14 |
|  | AO1 / AO2 | los dos cables del motor | — |
|  | BIN1, BIN2, PWMB | GND | — |
|  | BO1 / BO2 | sin conectar | — |
| **Buzzer pasivo — opción A: directo** | + (señal) | GP15 (pin 20) | 20 |
|  | − (GND) | GND común | — |
| **Buzzer pasivo — opción B: con transistor NPN** | GPIO | GP15 (pin 20) → 1 kΩ → base del NPN | 20 |
|  | Emisor | GND común | — |
|  | Colector | pata − del buzzer | — |
|  | Pata + del buzzer | 3V3 de la fuente externa | — |
| **Botones (pulsadores, uno por función)** | B1 · medir (igual que el SW) | GP14 (pin 19) ← salida del módulo de botones | 19 |
|  | B2 · calibrar / abrir el menú | GP20 (pin 26) ← salida del módulo de botones | 26 |
|  | B3 · bajar en el menú | GP21 (pin 27) ← salida del módulo de botones | 27 |
|  | B4 · menú (corto) / guardar (mantenido) | GP0 (pin 1) ← salida del módulo de botones | 1 |
| **Joystick analógico (tipo KY-023)** | VCC (+5V) | 3V3 de la fuente externa | — |
|  | GND | GND común | — |
|  | VRx / JX | GP26 (pin 31) | 31 |
|  | VRy / JY | GP27 (pin 32) | 32 |
|  | SW | GP22 (pin 29) | 29 |

«Pin físico» es el número del conector de 40 pines de la Pico W; «—» indica que no va a la Pico
(alimentación, masa, componentes externos o conexiones fijas). Los detalles y las notas de cada
módulo están en la sección 2.

> ⚠ **El joystick está serigrafiado «+5V»: conéctalo a 3V3, nunca a 5 V** (los GPIO no lo toleran).
> ⚠ **`VM` del TB6612 no puede salir del 3V3**: necesita su propia fuente (≈ 4,5–13,5 V según el datasheet de Toshiba; verificarlo),
> con la GND unida al GND común.

## 1. Alimentación y masas

| De | A | Nota |
|---|---|---|
| Fuente externa **+3V3** | VCC de **todos** los módulos (ST7789, TCS3200, 3 IR, TB6612 lógica, joystick) | un solo bus de 3V3 |
| Fuente externa **GND** | GND de todos los módulos **y** un GND de la Pico (pin 3, 8, 13, 18, 23, 28 o 38) | **masa común obligatoria** |
| Fuente del **motor** (+) | **VM** del TB6612FNG | fuente aparte; su **GND también va al GND común** |
| Pico W | **USB**, o bien **VSYS (pin 39) a través de un diodo Schottky** desde la fuente externa | VSYS admite 1,8–5,5 V |
| ❌ **NO conectar** | el +3V3 de la fuente al pin **36 (3V3 OUT)** de la Pico | pondría dos fuentes en paralelo |

Enciende la Pico y los módulos a la vez, o la Pico primero: un módulo alimentado con la Pico apagada
empuja corriente por sus salidas hacia GPIO sin alimentar.

## 2. Conexiones por módulo

### 2.1 Pantalla ST7789 2" 230×240 (con CS y BL)

| Pin del módulo | Se conecta a | Nota |
|---|---|---|
| GND | GND común |  |
| VCC | 3V3 de la fuente externa |  |
| SCL | **GP18** (pin 24) | es el reloj SPI0 SCK, no I2C |
| SDA | **GP19** (pin 25) | es MOSI (SPI0 TX), no I2C |
| RES | **GP17** (pin 22) |  |
| DC | **GP16** (pin 21) |  |
| CS | GND (fijo) | no ocupa GPIO: el módulo queda siempre seleccionado |
| BL | 3V3 (fijo) | HIPÓTESIS: backlight activo en ALTO, siempre encendido. Si la pantalla no se ve, pasar BL a GND |

> Pinout real del módulo: GND VCC SCL SDA RST DC CS BL. Con CS a GND se trabaja en SPI modo 3, como antes.

### 2.2 Sensor de color TCS3200

| Pin del módulo | Se conecta a | Nota |
|---|---|---|
| VCC | 3V3 de la fuente externa |  |
| GND | GND común |  |
| S0 | 3V3 (fijo) | S0=H, S1=L → escala 20 %. HIPÓTESIS |
| S1 | GND (fijo) | no ocupa GPIO |
| LED | 3V3 (fijo) | enciende los LED blancos del módulo; no ocupa GPIO |
| S2 | **GP2** (pin 4) | selección de filtro |
| S3 | **GP3** (pin 5) | selección de filtro |
| OUT | **GP5** (pin 7) | debe ir a un GPIO IMPAR: lo cuenta el PWM2 canal B |

> Módulo de 10 pines: GND OUT S2 S3 VCC / VCC S1 S0 LED GND. Los dos VCC y los dos GND están unidos entre sí.

### 2.3 IR final de carrera INICIO

| Pin del módulo | Se conecta a | Nota |
|---|---|---|
| VCC | 3V3 de la fuente externa |  |
| GND | GND común |  |
| OD | **GP4** (pin 6) | salida digital. PENDIENTE confirmar cuál de OA/OD es la digital |
| OA | sin conectar |  |

### 2.4 IR final de carrera FIN

| Pin del módulo | Se conecta a | Nota |
|---|---|---|
| VCC | 3V3 de la fuente externa |  |
| GND | GND común |  |
| OD | **GP6** (pin 9) | salida digital. PENDIENTE confirmar cuál de OA/OD es la digital |
| OA | sin conectar |  |

### 2.5 IR de proximidad (junto al TCS3200)

| Pin del módulo | Se conecta a | Nota |
|---|---|---|
| VCC | 3V3 de la fuente externa |  |
| GND | GND común |  |
| OA | **GP28** (pin 34) | analógica → ADC2 (distancia). HIPÓTESIS |
| OD | **GP1** (pin 2) | digital (hay objeto). HIPÓTESIS |

> Polaridades y umbrales se miden con PRUEBA_PROXIMIDAD (docs/hardware.md §10).

### 2.6 Driver de motor TB6612FNG + motor DC

| Pin del módulo | Se conecta a | Nota |
|---|---|---|
| VM | alimentación del MOTOR (fuente aparte) | NO del 3V3: el TB6612 pide ≈ 4,5–13,5 V; en el banco va a 5 V (el motor solo se mueve bien por encima del 90 % de ciclo de trabajo) |
| VCC | 3V3 de la fuente externa | lógica |
| GND | GND común | obligatoria: la masa del motor y la de la lógica deben unirse |
| STBY | **GP7** (pin 10) | además, resistencia de 10 kΩ a GND (ver «Componentes») |
| PWMA | **GP8** (pin 11) | PWM4 canal A, 20 kHz |
| AIN1 | **GP9** (pin 12) | sentido |
| AIN2 | **GP10** (pin 14) | sentido |
| AO1 / AO2 | los dos cables del motor | si gira al revés: invertir en MOTOR_SIGNO_HACIA_FIN |
| BIN1, BIN2, PWMB | GND | canal B sin usar |
| BO1 / BO2 | sin conectar |  |

> Probar SIEMPRE con la tarea de prueba del motor y el corte de alimentación a mano.

### 2.7 Buzzer pasivo — opción A: directo

| Pin del módulo | Se conecta a | Nota |
|---|---|---|
| + (señal) | **GP15** (pin 20) | PWM7 canal B |
| − (GND) | GND común |  |

> Solo si el buzzer es piezoeléctrico y consume poco (un GPIO da ≈ 12 mA).

### 2.8 Buzzer pasivo — opción B: con transistor NPN

| Pin del módulo | Se conecta a | Nota |
|---|---|---|
| GPIO | **GP15** (pin 20) → 1 kΩ → base del NPN | S8050 / 2N2222 / BC547 |
| Emisor | GND común |  |
| Colector | pata − del buzzer |  |
| Pata + del buzzer | 3V3 de la fuente externa | diodo 1N4148 en paralelo si el buzzer es magnético |

> Recomendada si el buzzer suena bajo o la Pico se reinicia al sonar.

### 2.9 Botones (pulsadores, uno por función)

| Pin del módulo | Se conecta a | Nota |
|---|---|---|
| B1 · medir (igual que el SW) | **GP14** (pin 19) ← salida del módulo de botones | pull-down interno |
| B2 · calibrar / abrir el menú | **GP20** (pin 26) ← salida del módulo de botones | pull-down interno |
| B3 · bajar en el menú | **GP21** (pin 27) ← salida del módulo de botones | pull-down interno |
| B4 · menú (corto) / guardar (mantenido) | **GP0** (pin 1) ← salida del módulo de botones | pull-down interno |

> **Activos en ALTO**: el módulo de botones lleva una resistencia a GND y cada botón lleva su salida a 3V3 (medido en el banco: con la lógica en bajo se leían los cuatro pulsados). Con cables largos y pulsaciones fantasma: 100 nF entre el pin y GND.

### 2.10 Joystick analógico (tipo KY-023)

| Pin del módulo | Se conecta a | Nota |
|---|---|---|
| VCC (+5V) | 3V3 de la fuente externa | NUNCA 5 V: los GPIO no lo toleran |
| GND | GND común |  |
| VRx / JX | **GP26** (pin 31) | ADC0 |
| VRy / JY | **GP27** (pin 32) | ADC1 |
| SW | **GP22** (pin 29) | pulsador activo en bajo; pull-up interno; hace lo mismo que B1 |

> Mantener el joystick quieto al arrancar: mide su centro en ese momento.

## 3. Mapa por GPIO (para cablear siguiendo el conector)

| Pin físico | GPIO | Señal | Función |
|---:|---|---|---|
| 1 | GP0 | Botón 4 · menú / guardar | entrada, pull-down (activo en alto) |
| 2 | GP1 | IR proximidad · OD | entrada |
| 4 | GP2 | TCS3200 · S2 | salida |
| 5 | GP3 | TCS3200 · S3 | salida |
| 6 | GP4 | IR final INICIO | entrada |
| 7 | GP5 | TCS3200 · OUT | PWM2 B (contador) |
| 9 | GP6 | IR final FIN | entrada |
| 10 | GP7 | TB6612 · STBY | salida |
| 11 | GP8 | TB6612 · PWMA | PWM4 A |
| 12 | GP9 | TB6612 · AIN1 | salida |
| 14 | GP10 | TB6612 · AIN2 | salida |
| 19 | GP14 | Botón 1 · medir | entrada, pull-down (activo en alto) |
| 20 | GP15 | Buzzer | PWM7 B |
| 21 | GP16 | ST7789 · DC | salida |
| 22 | GP17 | ST7789 · RES | salida |
| 24 | GP18 | ST7789 · SCK | SPI0 SCK |
| 25 | GP19 | ST7789 · SDA | SPI0 TX |
| 26 | GP20 | Botón 2 · calibrar / menú | entrada, pull-down (activo en alto) |
| 27 | GP21 | Botón 3 · bajar en el menú | entrada, pull-down (activo en alto) |
| 29 | GP22 | Joystick · SW (= B1) | entrada, pull-up (activo en bajo) |
| 31 | GP26 | Joystick · JX | ADC0 |
| 32 | GP27 | Joystick · JY | ADC1 |
| 34 | GP28 | IR proximidad · OA | ADC2 |

Quedan libres **GP11, GP12 y GP13** (antes el LED RGB); el resto de los GPIO utilizables está asignado. **No usar** GP23, GP24, GP25 ni GP29:
los ocupa el chip Wi-Fi (CYW43) de la Pico W. Los pines 3, 8, 13, 18, 23, 28, 33 y 38 son GND.

## 4. Componentes externos recomendados

| Dónde | Componente | Para qué |
|---|---|---|
| TB6612 `STBY` ↔ GND | resistencia **10 kΩ** | mantiene el puente apagado mientras el GPIO está sin configurar (arranque) |
| TB6612 `VM` ↔ GND | electrolítico **≥ 470 µF** + cerámico 100 nF, lo más cerca del módulo | picos de arranque del motor |
| Junto a cada módulo (VCC ↔ GND) | cerámico **100 nF** | ruido del motor sobre el TCS3200 y el ADC |
| Fuente → VSYS de la Pico | diodo **Schottky** | evita retroalimentar a la fuente cuando se conecta el USB |

## 5. Diagrama del conector (Pico W vista desde arriba, USB arriba)

```
                 +--USB--+
      Botón 4 · menú / guardar GP0  1 |      | 40 VBUS
            IR proximidad · OD GP1  2 |      | 39 VSYS
                               GND  3 |      | 38 GND
                  TCS3200 · S2 GP2  4 |      | 37 3V3_EN
                  TCS3200 · S3 GP3  5 |      | 36 3V3(OUT)
               IR final INICIO GP4  6 |      | 35 ADC_VREF
                 TCS3200 · OUT GP5  7 |      | 34 GP28 IR proximidad · OA
                               GND  8 |      | 33 AGND
                  IR final FIN GP6  9 |      | 32 GP27 Joystick · JY
                 TB6612 · STBY GP7 10 |      | 31 GP26 Joystick · JX
                 TB6612 · PWMA GP8 11 |      | 30 RUN
                 TB6612 · AIN1 GP9 12 |      | 29 GP22 Joystick · SW (= B1)
                               GND 13 |      | 28 GND
                TB6612 · AIN2 GP10 14 |      | 27 GP21 Botón 3 · bajar en el menú
                      (libre) GP11 15 |      | 26 GP20 Botón 2 · calibrar / menú
                      (libre) GP12 16 |      | 25 GP19 ST7789 · SDA
                      (libre) GP13 17 |      | 24 GP18 ST7789 · SCK
                               GND 18 |      | 23 GND
              Botón 1 · medir GP14 19 |      | 22 GP17 ST7789 · RES
                       Buzzer GP15 20 |      | 21 GP16 ST7789 · DC
```

## 6. Antes de dar corriente

1. Comprobar con multímetro que **no hay continuidad entre +3V3 y GND**.
2. Comprobar que **GND de la fuente, de los módulos, de la fuente del motor y de la Pico** están unidos.
3. Con el motor **desconectado**, cargar la prueba de los finales de carrera y verificar los IR a mano
   (`prueba_motor.c`, paso 0). Después, el pulso corto de prueba del motor.
4. Verificar la tensión de cada pin de alimentación de módulo: **3,3 V**, nunca 5 V.

