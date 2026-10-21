# Detector de color con carrito lineal (cálido / frío / neutro)

Proyecto final de **Arquitectura II (UMG)**. Firmware en **C** para una **Raspberry Pi Pico W**
(RP2040) que mide el color de una muestra con un sensor **TCS3200**, lo convierte a tono (HSV) y
clasifica el color como **cálido**, **frío** o **neutro** y mueve un carrito sobre un eje lineal X
(poleas y correa, motor DC con TB6612FNG) hasta la posición de esa temperatura. Muestra el resultado en una pantalla **ST7789 240×320**.

> **Requisito del curso:** todo acceso a pines y periféricos se hace a través de `struct` volátiles
> que describen el mapa de memoria del RP2040 (`src/hw/`, prefijo `rp_` / `RP_`). **No se usa la
> HAL del SDK** (`gpio_put`, `spi_write_blocking`, `pwm_set_*`, `adc_read`…). El SDK solo aporta el
> arranque (boot2, crt0), los relojes y `printf` por USB.

## Integrantes — Grupo 1

| Nombre | Carné |
|---|---|
| Brayan Kenet Rivera Quinilla | 9490-23-2835 |
| Wilder Alfonso Castillo Salguero | 9490-21-3708 |
| Erick Orlando Rashon Gonzalez | 9490-11-5609 |
| Marco Tulio Pineda Recinos | 9490-23-2906 |
| Werner Josue Ortiz Abdalla | 9040-20-1143 |

## Hardware

| Módulo | Función |
|---|---|
| Raspberry Pi Pico W | Microcontrolador (GP23/24/25/29 reservados por el chip Wi-Fi) |
| TCS3200 (módulo con pin LED) | Sensor de color; la frecuencia se mide por hardware (PWM2 B como contador de flancos). Pines del módulo: VCC, GND, S0, S1, S2, S3, OUT y LED; S0/S1 y LED van fijos por cable (sin GPIO) |
| ST7789 2" 240×320 GMT020-02 (CS a GND, BL a 3V3) | Interfaz gráfica por SPI0, modo 3 |
| TB6612FNG + motor DC | Desplazamiento del carrito |
| 2 × IR | Finales de carrera (inicio / fin) |
| 1 × IR | Presencia y distancia de la muestra |
| 4 botones + joystick | Medir, menú y navegación, guardar; el SW del joystick hace lo mismo que B1 |
| Buzzer pasivo | Realimentación al usuario |

Todos los módulos se alimentan a **3V3 desde una fuente externa**, con **GND común** con la Pico.
El `VM` del TB6612 necesita ~4,5–13,5 V: **no** puede salir de la fuente de 3V3. En el banco va a **5 V**
(con eso el motor solo se mueve bien por encima del 90 % de ciclo de trabajo). Los botones B1–B4 son
**activos en alto** y el SW del joystick, en bajo.

- Asignación de pines: `src/configuracion/pines.h` (con `_Static_assert` que detecta conflictos).
- Para cablear: [`docs/conexiones.md`](docs/conexiones.md) (tabla por módulo, mapa por GPIO y
  diagrama del conector). Se genera con `python tools/generar_conexiones.py`; no editar a mano.
- Razonamiento, hipótesis y decisiones pendientes: [`docs/hardware.md`](docs/hardware.md).
- Reparto del trabajo por temas entre los integrantes: [`docs/reparto-trabajo.md`](docs/reparto-trabajo.md).

## Cómo funciona

1. **Al encender:** 2 s después de arrancar, el sistema arma el motor y **referencia el eje solo**: busca el
   final de INICIO, mide el recorrido y vuelve. Funciona desde cualquier posición del carrito (por si se
   movió en el transporte), reintenta si falla por algo recuperable y, si se desconecta un final de carrera,
   se recupera sola al reconectarlo. Mientras tanto la pantalla abre el **asistente de inicio** para calibrar
   el negro y el blanco.
2. **Medir:** la guía espera a que el IR de proximidad vea la muestra a la distancia correcta
   (300 ms continuos), promedia 3 lecturas del TCS3200 y aplica la calibración negro/blanco. El IR no ve las
   muestras oscuras: **B1 mantenido** mide sin esperarle. Suena un barrido de escáner mientras mide.
3. **Clasificar:** RGB → HSV → temperatura (`dominio/escala.c`). Los tonos de 315° a 75° (rojo, naranja,
   amarillo, magenta rojizo) son **cálidos**; de 75° a 315° (verde, cian, azul, violeta), **fríos**; los
   colores sin tono (negro, gris, blanco) son **neutros**.
   Cada temperatura tiene una posición fija en el eje X: cálido a 1/6, neutro a 1/2 y frío a 5/6 del recorrido
   de trabajo (`configuracion/escala.h`).
4. **Posicionar:** el carrito va a la posición **por tiempo** (velocidad × dt), resincronizando
   en cada final de carrera. Con el recorrido real de ≈ 0,75 s la precisión de las paradas **aún no está
   medida** en el banco (la simulación predice errores de hasta ≈ 30 mm).

Máquina de estados (`aplicacion/sistema.c`): `INIT → HOMING → IDLE ↔ MEASURE / CALIBRATE /
MOVE_TO_TARGET / MANUAL`, con `ERROR` recuperable.

## Controles

| Pantalla | Botones |
|---|---|
| Principal | **B1 o SW** medir · **B2** abre el menú de calibración · **B3** no hace nada · **B4 corto** abre el menú · **B4 mantenido** guarda la lectura · B1/SW mantenido fuerza la medición |
| Menú e historial | **B2/B3** o el joystick arriba/abajo navegan · **B1/SW** confirman · **B4** sale |
| Mover cursor | joystick X ajusta los mm · **B1/SW** envía el carrito · **B4** cancela |
| Modo manual | joystick X tono, Y saturación · **B1/SW** o **B4** salen |
| Asistente | **B1/SW** calibran y avanzan · **B2** calibra sin esperar al IR · **B4 largo** lo omite |
| Operación en curso | **B4** la cancela |
| ERROR | **B4 largo** lo reinicia |

La pantalla principal muestra el color, sus valores R/G/B y H/S/V, la posición y el objetivo del carrito, la
escala de **tres zonas** (cálido / neutro / frío) con las marcas de posición, una **barra cromática** de 0° a 360° con
un cursor en el tono medido, el estado del eje y de los sensores y un mensaje. La pantalla «Información»
incluye también el nombre del programa y los **integrantes del grupo**.

## Estructura

```
src/
  main.c            elige qué se ejecuta (PRUEBA_ACTIVA)
  hw/               mapa de memoria y registros del RP2040 como structs, pines, tiempo
  drivers/          st7789, tcs3200, adc, botones, joystick, buzzer, motor, finales, proximidad
  dominio/          lógica pura: color, escala, selector de color, posición, límites, historial, sonidos, proximidad
  aplicacion/       carro, eje, deteccion, sistema (máquina de estados), protocolo serie, app
  ui/               interfaz de 6 pantallas (principal, menú, historial, mover, info, asistente)
  graficos/         texto UTF-8 y fuente 5×7 (generada)
  configuracion/    pines, escala, eje, motor, proximidad, actuadores…
  pruebas/          programas de prueba por módulo
test/host/          pruebas en el PC con el código real y hardware simulado
tools/              generadores (fuente, tabla de conexiones)
docs/               hardware, conexiones, compilar y depurar
```

`dominio/` es lógica pura sin dependencias de hardware, por eso se puede ejecutar tal cual en el PC.

## Compilar y cargar

Requiere la extensión *Raspberry Pi Pico* de VS Code (SDK 2.3.1, toolchain `arm-none-eabi` 15.2.Rel1,
CMake, Ninja). El bloque `# == DO NOT EDIT ... ==` de `CMakeLists.txt` lo regenera la extensión.

```powershell
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
ninja -C build
picotool load build/detector_color.uf2 -fx     # Pico en BOOTSEL por USB
```

También con las tareas de VS Code (**Compile Project**, **Run Project**, **Flash**) o depurando con
una sonda CMSIS-DAP. Detalle en [`docs/compilar-y-depurar.md`](docs/compilar-y-depurar.md).

### Depuración con una segunda RP2040 (debugprobe)

Se depura con **otra RP2040 (Pico) cargada con el firmware `debugprobe`**, que se comporta como
sonda CMSIS-DAP por SWD. `launch.json` ya usa `interface/cmsis-dap.cfg` + `target/rp2040.cfg`.

| Pico sonda (debugprobe) | Pico W objetivo |
|---|---|
| GP2 (SWCLK) | SWCLK (pad de depuración) |
| GP3 (SWDIO) | SWDIO (pad de depuración) |
| GND | GND |
| GP4 / GP5 (UART, opcional) | no se usa: la salida va por USB CDC |

- Los pines de la sonda son los del `debugprobe` estándar (GP2/GP3); verificar contra la versión
  del firmware que se cargó.
- El GND de la sonda, el del objetivo y el de la fuente externa de 3V3 deben ser **el mismo**.
- No alimentar el objetivo desde la sonda ni desde la fuente al mismo tiempo que por USB sin diodo
  (ver la nota de VSYS/Schottky en `docs/conexiones.md`).
- Iniciar la sesión con *Pico Debug (CMSIS-DAP)* en VS Code, o cargar con la tarea **Flash**.

Al añadir un fuente nuevo hay que sumarlo a `add_executable(...)`; no hay globbing.

### Qué se carga

`PRUEBA_ACTIVA` en `src/main.c` elige el programa; cambiarlo y recompilar. La salida de texto sale
**por USB CDC** (no hay UART).

| Valor | Programa |
|---|---|
| 1 | `PRUEBA_PANTALLA` |
| 2 | `PRUEBA_TCS3200` (teclas `n` / `b` / `c` por USB) |
| 3 | `PRUEBA_ENTRADAS` (botones y joystick) |
| 4 | `PRUEBA_ACTUADORES` (buzzer: B1–B4 y SW recorren todos los sonidos) |
| 5 | `PRUEBA_MOTOR` |
| 6 | `PRUEBA_EJE` (homing, posiciones, repetibilidad) |
| 7 | `PRUEBA_MAPEO` (color → posición; funciona sin motor) |
| 8 | `PRUEBA_PROXIMIDAD` (mide polaridad y umbrales del IR e imprime las líneas para `configuracion/proximidad.h`) |
| **9** | **`MODO_APLICACION`** — la aplicación completa (valor por defecto) |

## Asistente de inicio

La calibración del color (negro y blanco) vive en RAM y se pierde al cortar la corriente. Por eso, al
encender, la pantalla abre un asistente de tres pasos: **1) referenciar el eje** (se hace solo),
**2) calibrar el negro** y **3) calibrar el blanco**, con las instrucciones en pantalla. **B1** calibra
(el negro se mide sin esperar al IR de proximidad, que no ve las muestras oscuras; **B2** fuerza también
el blanco) y **B4 largo** omite el asistente. **B1** y el **SW** del joystick son el mismo botón. Los pasos se deducen del
estado del sistema, no de una memoria propia. Se desactiva con `ASISTENTE_AL_INICIAR` en `src/configuracion/sistema.h`.

## Modo manual (selector de color)

En el menú, «Modo manual» (con el eje referenciado) convierte el joystick en un **selector de color**:
**X** mueve una marca por la barra cromática (tono de 0° a 360°, dando la vuelta) e **Y** sube o baja la
saturación; sin saturación el color es un gris, es decir, **neutro**. La pantalla muestra el color elegido
como si fuera una lectura, y cuando su temperatura (cálido, neutro o frío) se mantiene un cuarto de segundo,
el carrito va solo a la posición de esa temperatura. **B1/SW** o **B4** salen. La lógica está en
`dominio/selector.c`.

## Puerto serie (USB)

La aplicación habla un protocolo de texto por el puerto serie USB (115200, una línea por mensaje,
terminada en salto de línea; sirve cualquier terminal serie, p. ej. PuTTY o el monitor de VS Code).
Está en `src/aplicacion/protocolo.c` (documentado en `protocolo.h`); no usa el SDK: `main.c` le
pasa los bytes recibidos.

| Comando | Qué hace |
|---|---|
| `AYUDA`, `ESTADO`, `LECTURA`, `HIST` | consultas: ayuda, estado completo, última lectura e historial |
| `MEDIR`, `CAL NEGRO`, `CAL BLANCO`, `FORZAR` | medir, calibrar y saltarse el IR de proximidad (muestras oscuras) |
| `ARMAR`, `DESARMAR`, `REF`, `IR <mm>`, `PARAR`, `REINICIAR` | motor y eje: armar, referenciar, ir a una posición (`IR 160.5`), cancelar y reiniciar un error |
| `GUARDAR` | guarda la última lectura en el historial |

Cada comando responde `OK` o `ERR,<motivo>` (`OCUPADO`, `NO_ARMADO`, `SIN_REFERENCIA`, `NO_CALIBRADO`,
`ERROR_ACTIVO`, `ARGUMENTO`, `FUERA_DE_RANGO`, `COMANDO`…). Además el sistema envía por su cuenta
`HOLA,…` al arrancar y una línea `EVT,<evento>` por cada evento (`REFERENCIADO`, `ANALIZANDO`,
`MEDIDA,hex=…,rgb=…,hsv=…,hz=…,temp=CALIDO|FRIO|NEUTRO,obj_mm=…`, `CAL_NEGRO`, `LLEGADA`, `ERROR,…`…).

Ejemplo: `MEDIR` → `OK`, `EVT,ANALIZANDO`, `EVT,MEDIDA,hex=#264420,rgb=38,68,32,hsv=109,137,68,…,temp=FRIO,obj_mm=266.7`.

## Pruebas en el PC

Compilan y ejecutan el mismo código del firmware con hardware simulado (motor, finales, ADC,
TCS3200, pantalla). Usan el `gcc` de `D:\dev\w64devkit\bin`.

```bash
test/host/compilar.sh                 # simulación del eje, autoreferenciar (22), protocolo (30), máquina de estados (104) y pantallas a PNG
RUIDO=0.05 test/host/compilar.sh      # ruido de ±5 % en la velocidad del motor
```

Ver [`test/host/LEEME.md`](test/host/LEEME.md). Hoy la máquina de estados da **3 fallos de precisión conocidos**: son la
simulación del carrito con el recorrido real de ≈ 0,75 s (ver «Estado»).

## Estado

El firmware compila sin avisos y se ha **probado en la placa** (2026-10-20):

- Pantalla, botones (B1–B4 activos en alto), joystick, buzzer, TCS3200 y detección de color (rojo, verde, azul),
  IR de proximidad (con sus límites: satura y no ve el negro) y finales de carrera de INICIO y FIN.
- Motor con VM = 5 V: sentido hacia el FIN correcto, potencia mínima 90 %, recorrido completo de **320 mm** (medido con regla) en ≈ 0,75 s.
- Referencia automática al encender desde la posición en que esté el carrito, recuperación al desconectar y reconectar un final
  de carrera, asistente de inicio, modo manual (selector de color), protocolo serie por USB y menú con B2/B3.

**Pendiente de verificar en el banco:**

- **Precisión de las paradas** con regla (cálido 53,3 mm, neutro 160,0 mm, frío 266,7 mm desde el INICIO). Medido hasta ahora: el neutro
  quedó en 159,8 y 160,0 mm (error ≤ 0,2 mm en dos repeticiones); **faltan el cálido y el frío** y más repeticiones, también llegando desde el
  INICIO. La simulación, con este recorrido tan rápido, predecía errores de hasta ≈ 30 mm y no se ha cumplido en el neutro.
- Arranque con el carrito **pegado al FIN** y **pegado al INICIO**, y que no golpee los topes.
- **Subir VM** (el TB6612 admite hasta ≈ 13,5 V) para ir más lento y poder bajar la potencia mínima.
- Escala real de S0/S1 del TCS3200 (≈ 12 kHz de máximo con blanco) y si el IR de proximidad altera la lectura.
- Desconectar solo el INICIO a medio recorrido: no da error y puede descolocar la posición estimada.

Sin encoder, un final de carrera estropeado haría que el motor empuje contra el tope (≈ 2 s en
simulación); el timeout de `carro.c` limita ese riesgo.

## Pendiente / versiones futuras

- Guardar calibración de color e historial en flash (así el asistente solo haría falta la primera vez).
- Encoder o marcas en el eje para cerrar el lazo de posición.
- **Wi-Fi** (página de estado) y **UART físico** (GP0/GP1) por registros: ver `docs/hardware.md` §12.
- Herramienta de diagnóstico aparte con el mismo pin-out: `../prueba-motor-pantalla/` (motor y pantalla, con el multímetro).
- **v1.2:** envío de mediciones por Bluetooth (Pico W maestro SPP → HC-06). Pospuesto por decisión
  del equipo; ver `docs/hardware.md` §12.
