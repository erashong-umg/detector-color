# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Qué es esto

Proyecto final de Arquitectura II (UMG), Grupo 1. Firmware en **C11** para una **Raspberry Pi Pico W** (RP2040): mide el color con un TCS3200, lo pasa a tono (HSV) y mueve un carrito por un riel hasta la posición que le toca en una escala impresa; muestra el resultado en una ST7789 240×240. El README y `docs/` están en español, igual que el código, los comentarios y los identificadores.

`detector-color/` es el repositorio git. La carpeta padre (`Proyecto_Final_ARQ_II/`) solo contiene las guías `.docx` del curso, sin código.

## Regla central del curso (no negociable)

Todo acceso a pines y periféricos va por `struct` volátiles que describen el mapa de memoria del RP2040 (`src/hw/rp2040_regs.h`, prefijo `rp_` / `RP_`). **No usar la HAL del SDK** (`gpio_put`, `spi_write_blocking`, `pwm_set_*`, `adc_read`, `sleep_ms`…). Del SDK solo se usa `pico_stdlib` para arranque (boot2, crt0), relojes y `printf` por USB; `main.c` es el único sitio que incluye `pico/stdlib.h`. Para pines y tiempo usar `hw/pin.h` y `hw/tiempo.h`.

## Comandos

Compilar el firmware (el bloque `# == DO NOT EDIT ... ==` de `CMakeLists.txt` lo regenera la extensión de VS Code; no tocarlo):

```powershell
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
ninja -C build
picotool load build/detector_color.uf2 -fx     # Pico en BOOTSEL por USB
```

También valen las tareas de VS Code (**Compile Project**, **Run Project**, **Flash**, **Rescue Reset**) y la depuración *Pico Debug (CMSIS-DAP)* con una segunda Pico como `debugprobe`. Detalle en `docs/compilar-y-depurar.md`.

- Desde **Git Bash** el `PATH` resuelve `arm-none-eabi-gcc` a un GCC 4.7 viejo (`C:\yagarto-...`); compilar desde PowerShell/terminal de VS Code, o con las rutas de `~/.pico-sdk` por delante.
- Si un `build/` quedó configurado con otro generador (síntoma: `Does not match the generator used previously`), borrarlo entero; no se arregla reconfigurando.
- No hay globbing: **todo `.c` nuevo debe añadirse a `add_executable(...)`** en `CMakeLists.txt`.

Pruebas en el PC (sin Pico), con el `gcc` de `D:\dev\w64devkit\bin`:

```bash
test/host/compilar.sh                  # UI -> PNG, simulación del eje y test_sistema (86 comprobaciones)
RUIDO=0.05 test/host/compilar.sh       # ruido de ±5 % en la velocidad del motor
```

El script compila tres ejecutables (`harness_ui`, `harness_eje`, `test_sistema`) con `set -e`. No hay filtro para un caso suelto: para ejecutar uno, copiar la línea `gcc` correspondiente de `compilar.sh`. `test_sistema.exe` devuelve código distinto de cero si algo falla. Al añadir un fuente de `dominio/` o `aplicacion/` que use alguno de ellos, hay que sumarlo también a la línea `gcc` de `compilar.sh`.

La verificación en hardware es empírica, leyendo `printf` por USB CDC (no hay UART). **Nada se ha probado aún en el banco**; la lógica solo está validada contra simulaciones (ver "Estado" del README).

## Arquitectura

Capas, de abajo arriba, con la dependencia siempre hacia abajo:

```
hw/            registros del RP2040 como structs, pin.c, tiempo.h (único contacto con el silicio)
drivers/       un módulo físico cada uno: st7789, tcs3200, adc, botones, joystick, buzzer, led, motor, finales, proximidad
dominio/       lógica pura SIN hardware: color, escala, posicion, limites, historial, sonidos, eje, proximidad
aplicacion/    carro, eje, deteccion, sistema (máquina de estados), app (bucle principal)
ui/, graficos/ interfaz de pantallas, texto UTF-8 y fuente 5×7 (fuente5x7.c es generada)
configuracion/ constantes de hardware y calibración (pines.h, escala.h, eje.h, motor.h, tcs3200.h…)
pruebas/       programas de bring-up por módulo
```

- **`dominio/` y `aplicacion/` no incluyen cabeceras del SDK ni de `hw/`**: por eso el mismo código corre en el PC. Para probarlo, `test/host/` sustituye solo lo que toca hardware (mocks de `st7789`, `motor`, `finales`, `adc`, `tcs3200`, y `mock/hw/{pin,tiempo}.h` que tapan `hw/`). Mantener esa frontera: si un módulo de `dominio/` necesita hardware, es que pertenece a `drivers/`.
- **`main.c` elige qué corre** con `PRUEBA_ACTIVA` (1–8 programas de `pruebas/`, 9 = `MODO_APLICACION`, el valor por defecto). Cambiarlo y recompilar; *Run Project* y *Flash* sobrescriben lo que haya cargado en la Pico.
- **Máquina de estados** (`aplicacion/sistema.c`): `INIT → HOMING → IDLE ↔ MEASURE / CALIBRATE / MOVE_TO_TARGET / MANUAL`, con `ERROR` recuperable. `sistema` es el orquestador del dominio pero **no toca pantalla, botones, buzzer ni LED**: la app le da órdenes (`sistema_*`) y recoge eventos (`sistema_tomar_evento`) y guía (`sistema_guia`). `sistema_actualizar()` se llama en cada vuelta del bucle (≤ 1 ms); medir bloquea ≈270 ms, por eso solo ocurre con el carrito parado.
- **Posición por tiempo, sin encoder**: el carrito estima su posición como velocidad × dt y se resincroniza en cada final de carrera IR; `carro.c` tiene un timeout que limita el riesgo de empujar contra el tope.
- **Medición de color**: la frecuencia del TCS3200 se cuenta por hardware con PWM2 B como contador de flancos; la medición solo arranca cuando el IR de proximidad ve la muestra a la distancia correcta (300 ms continuos). Se promedian 3 lecturas y se aplica calibración negro/blanco antes de RGB → HSV → µm (`dominio/escala.c`); los acromáticos no mueven el cursor y el rojo va al extremo más cercano.

## Pines y documentación generada

- Los pines se definen **solo** en `src/configuracion/pines.h` (con `_Static_assert` que detecta conflictos). GP23/24/25/29 están reservados por el chip Wi-Fi de la Pico W.
- `docs/conexiones.md` está **generado** por `python tools/generar_conexiones.py` desde `pines.h`; no editarlo a mano, regenerar. `src/graficos/fuente5x7.c` sale de `tools/generar_fuente.py`.
- Todos los módulos van a **3V3 de una fuente externa con GND común**; el `VM` del TB6612FNG necesita 4,5–13,5 V aparte. El LED RGB es de **ánodo común** (lógica invertida).

## Convenciones

- Código, comentarios, identificadores y salida por consola en español; archivos en minúsculas con guion bajo.
- Los módulos de `drivers/` y `dominio/` se prefijan con el nombre del módulo (`pin_*`, `tiempo_*`, `sistema_*`).
- Mensajes de commit: solo la descripción del cambio, sin trailers de herramienta (regla global del usuario).
- El reparto por integrante está en `docs/reparto-trabajo.md`; las decisiones y hipótesis de hardware pendientes, en `docs/hardware.md`.

## Rama `micropython` — `detector-color-py/`

Reescritura simple del firmware en **MicroPython** (carpeta aparte; el C de `src/` no se toca). Es plana: `app.py` (estados y bucle), `carro.py` (motor + finales + posición por tiempo), `sensores.py`, `actuadores.py`, `color.py` (lógica pura), `pantalla.py`/`ui.py`/`fuente.py`, `config.py` (pines y parámetros en mm). Usa la API `machine` y **rompe la regla «sin HAL»** del curso; el contador de flancos del TCS3200 es un programa PIO (`rp2`). Detalle en `detector-color-py/README.md`.

- Pruebas en el PC: `cd detector-color-py && python test_pc.py` (simula `machine`, `framebuf` y el reloj; si se añade una llamada a `machine`/`time`/`framebuf` que el simulador no tenga, hay que ampliar los fakes del propio test).
- Subir a la Pico: `mpremote cp main.py app.py carro.py sensores.py actuadores.py color.py pantalla.py ui.py fuente.py config.py :`.
- La pantalla se crea **antes** que los sensores: `SPI(0)` reserva GP4 (MISO por defecto), que es el IR de inicio.
- Todo el tiempo va con `time.ticks_*` y `ticks_diff` (el contador de MicroPython da la vuelta a los 2^30).
