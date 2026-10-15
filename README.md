# Detector de color con cursor cromático lineal

Proyecto final de **Arquitectura II (UMG)**. Firmware en **C** para una **Raspberry Pi Pico W**
(RP2040) que mide el color de una muestra con un sensor **TCS3200**, lo convierte a tono (HSV) y
mueve un carrito sobre un riel lineal hasta la posición que le corresponde en una escala de color
impresa. Muestra el resultado en una pantalla **ST7789 240×240**.

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
| TCS3200 | Sensor de color; la frecuencia se mide por hardware (PWM2 B como contador de flancos) |
| ST7789 240×240 (sin CS) | Interfaz gráfica por SPI0, modo 3 |
| TB6612FNG + motor DC | Desplazamiento del carrito |
| 2 × IR | Finales de carrera (inicio / fin) |
| 1 × IR | Presencia y distancia de la muestra |
| 4 botones + joystick | Medir, calibrar, posicionar, guardar/menú; navegación |
| LED RGB (ánodo común) y buzzer pasivo | Realimentación al usuario |

Todos los módulos se alimentan a **3V3 desde una fuente externa**, con **GND común** con la Pico.
El `VM` del TB6612 necesita ~4,5–13,5 V: **no** puede salir de la fuente de 3V3.

- Asignación de pines: `src/configuracion/pines.h` (con `_Static_assert` que detecta conflictos).
- Para cablear: [`docs/conexiones.md`](docs/conexiones.md) (tabla por módulo, mapa por GPIO y
  diagrama del conector). Se genera con `python tools/generar_conexiones.py`; no editar a mano.
- Razonamiento, hipótesis y decisiones pendientes: [`docs/hardware.md`](docs/hardware.md).

## Cómo funciona

1. **Referenciar (homing):** el carrito busca el final de carrera de inicio y mide el recorrido.
2. **Medir:** la guía espera a que el IR de proximidad vea la muestra a la distancia correcta
   (300 ms continuos), promedia 3 lecturas del TCS3200 y aplica la calibración negro/blanco.
3. **Mapear:** RGB → HSV → posición en µm (`dominio/escala.c`). Los colores acromáticos no mueven el
   cursor; la banda del rojo va al extremo más cercano para evitar saltos.
4. **Posicionar:** el carrito va a la posición **por tiempo** (velocidad × dt), resincronizando
   en cada final de carrera. Error típico en simulación: 1–4 mm.

Máquina de estados (`aplicacion/sistema.c`): `INIT → HOMING → IDLE ↔ MEASURE / CALIBRATE /
MOVE_TO_TARGET / MANUAL`, con `ERROR` recuperable.

## Estructura

```
src/
  main.c            elige qué se ejecuta (PRUEBA_ACTIVA)
  hw/               mapa de memoria y registros del RP2040 como structs, pines, tiempo
  drivers/          st7789, tcs3200, adc, botones, joystick, buzzer, led, motor, finales, proximidad
  dominio/          lógica pura: color, escala, posición, límites, historial, sonidos, proximidad
  aplicacion/       carro, eje, deteccion, sistema (máquina de estados), app
  ui/               interfaz de 5 pantallas (principal, menú, historial, mover, info)
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
| 4 | `PRUEBA_ACTUADORES` (buzzer y LED) |
| 5 | `PRUEBA_MOTOR` |
| 6 | `PRUEBA_EJE` (homing, posiciones, repetibilidad) |
| 7 | `PRUEBA_MAPEO` (color → posición; funciona sin motor) |
| 8 | `PRUEBA_PROXIMIDAD` (mide polaridad y umbrales del IR e imprime las líneas para `configuracion/proximidad.h`) |
| **9** | **`MODO_APLICACION`** — la aplicación completa (valor por defecto) |

## Pruebas en el PC

Compilan y ejecutan el mismo código del firmware con hardware simulado (motor, finales, ADC,
TCS3200, pantalla). Usan el `gcc` de `D:\dev\w64devkit\bin`.

```bash
test/host/compilar.sh                 # 86 comprobaciones + pantallas renderizadas a PNG
RUIDO=0.05 test/host/compilar.sh      # ruido de ±5 % en la velocidad del motor
```

Ver [`test/host/LEEME.md`](test/host/LEEME.md).

## Estado

Todo el firmware compila sin avisos y la lógica pasa las pruebas del PC, pero **nada está probado en
hardware todavía**. Pendiente de verificar en el banco:

- Sentido del motor, polaridad de los IR de fin de carrera y tiempo real de recorrido.
- Polaridad y umbrales del IR de proximidad (y si su LED altera la lectura del TCS3200).
- Fuente y corriente de `VM` del motor; corriente de bloqueo.
- Recorrido real entre finales y ancho/desplazamiento de la escala impresa.
- Si S0/S1 del TCS3200 van fijos por cable; si el buzzer va directo o con transistor.

Sin encoder, un final de carrera estropeado haría que el motor empuje contra el tope (3–11 s en
simulación); el timeout de `carro.c` limita ese riesgo.

## Pendiente / versiones futuras

- Guardar calibración de color e historial en flash.
- **v1.2:** envío de mediciones por Bluetooth (Pico W maestro SPP → HC-06). Pospuesto por decisión
  del equipo; ver `docs/hardware.md` §12.
