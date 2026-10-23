# Guía del proyecto

Notas para trabajar en este repositorio: cómo compilar y probar, la arquitectura y las convenciones.

## Qué es esto

Proyecto final de Arquitectura II (UMG), Grupo 1. Firmware en **C11** para una **Raspberry Pi Pico W** (RP2040): mide el color con un TCS3200, lo pasa a tono (HSV), lo clasifica como cálido, frío o neutro y mueve un carrito por un eje lineal X hasta la posición fija de esa temperatura; muestra el resultado en una ST7789 240×320. El README y `docs/` están en español, igual que el código, los comentarios y los identificadores.

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
test/host/compilar.sh                  # UI -> PNG, eje, autoreferenciar (22), protocolo (30) y test_sistema (112)
RUIDO=0.05 test/host/compilar.sh       # ruido de ±5 % en la velocidad del motor
```

El script compila cinco ejecutables (`harness_ui`, `harness_eje`, `test_autoref`, `test_protocolo`, `test_sistema`) con `set -e`: el primero que devuelve error corta el script, por eso `test_sistema` va el último (hoy da 2 fallos de precisión conocidos, rojo y blanco: el carrito simulado se queda corto con el recorrido real de ≈ 0,75 s). `test_autoref` y `test_protocolo` son el mismo `test_sistema.c` compilado con `-DSOLO_AUTOREF` / `-DSOLO_PROTOCOLO`. No hay filtro para un caso suelto: para ejecutar uno, copiar la línea `gcc` correspondiente de `compilar.sh`. `test_sistema.exe` devuelve código distinto de cero si algo falla. Al añadir un fuente de `dominio/` o `aplicacion/` que use alguno de ellos, hay que sumarlo también a la línea `gcc` de `compilar.sh`.

La verificación en hardware es empírica, leyendo el protocolo serie / `printf` por USB CDC (no hay UART). Ya se probó en la placa casi todo (ver «Estado» del README y «Medido en el banco» en `docs/hardware.md`); lo pendiente es la precisión de las tres paradas, arrancar pegado a los extremos y subir VM.

## Arquitectura

Capas, de abajo arriba, con la dependencia siempre hacia abajo:

```
hw/            registros del RP2040 como structs, pin.c, tiempo.h (único contacto con el silicio)
drivers/       un módulo físico cada uno: st7789, tcs3200, adc, botones, joystick, buzzer, motor, finales, proximidad
dominio/       lógica pura SIN hardware: color, escala, selector, posicion, limites, historial, sonidos, eje, proximidad
aplicacion/    carro, eje, deteccion, sistema (máquina de estados), protocolo (puerto serie), app (bucle principal)
ui/, graficos/ interfaz de pantallas, texto UTF-8 y fuente 5×7 (fuente5x7.c es generada)
configuracion/ constantes de hardware y calibración (pines.h, escala.h, eje.h, motor.h, tcs3200.h…)
pruebas/       programas de bring-up por módulo
```

- **`dominio/` y `aplicacion/` no incluyen cabeceras del SDK ni de `hw/`**: por eso el mismo código corre en el PC. Para probarlo, `test/host/` sustituye solo lo que toca hardware (mocks de `st7789`, `motor`, `finales`, `adc`, `tcs3200`, y `mock/hw/{pin,tiempo}.h` que tapan `hw/`). Mantener esa frontera: si un módulo de `dominio/` necesita hardware, es que pertenece a `drivers/`.
- **`main.c` elige qué corre** con `PRUEBA_ACTIVA` (1–8, 10 y 11 son programas de `pruebas/`, 9 = `MODO_APLICACION`, el valor por defecto; 10 = `prueba_isr` y 11 = `prueba_ciclos`, las pruebas de las Fases 2 y 3 del enunciado). Cambiarlo y recompilar; *Run Project* y *Flash* sobrescriben lo que haya cargado en la Pico.
- **Máquina de estados** (`aplicacion/sistema.c`): `INIT → HOMING → IDLE ↔ MEASURE / CALIBRATE / MOVE_TO_TARGET / MANUAL`, con `ERROR` recuperable. `sistema` es el orquestador del dominio pero **no toca pantalla, botones ni buzzer**: la app le da órdenes (`sistema_*`) y recoge eventos (`sistema_tomar_evento`) y guía (`sistema_guia`). `sistema_actualizar()` se llama en cada vuelta del bucle (≤ 1 ms); medir NO bloquea: `deteccion_empezar()` arranca la adquisición por interrupción (ALARM0 → `isr_irq0` en `drivers/tcs3200_async.c`) y `sistema` la sigue en cada vuelta con `deteccion_paso()` (≈ 3 × 88 ms); solo se arranca con el carrito parado.
- **Arranque y recuperación**: 2 s después de arrancar, `sistema` arma el motor y referencia el eje solo (`SISTEMA_AUTOREFERENCIAR`), desde cualquier posición, con reintentos si falla por algo recuperable. Si se desconecta un final (se lee como activo) y quedan los dos activos, el sistema entra en ERROR y, cuando vuelven a ser coherentes 0,5 s, se reinicia solo y vuelve a referenciar. La calibración negro/blanco vive en RAM: la pantalla abre un **asistente de inicio** (`ui_abrir_asistente`, `ASISTENTE_AL_INICIAR`) que guía referenciar → negro → blanco.
- **Botones**: B1–B4 son **activos en alto** (`BOTONES_ACTIVO_ALTO`); el SW del joystick es activo en bajo y `app.c` lo funde con B1 (`ui_entrada_t` no tiene un campo `ok`). En la principal B4 mantenido guarda y B4 corto abre el menú; B3 no hace nada. B2/B3 navegan los menús.
- **Modo manual**: es un selector de color (`dominio/selector.c`): el joystick mueve el tono (X) y la saturación (Y) y el carrito va a la posición de la temperatura resultante (`sistema_manual_ir_a`).
- **Sonidos**: `dominio/sonidos.c` (estilo CNC). El sonido de análisis sigue vivo durante la medición porque `app.c` registra `buzzer_actualizar` en la espera del TCS3200 (`deteccion_poner_espera`).
- **Posición por tiempo, sin encoder**: el carrito estima su posición como velocidad × dt y se resincroniza en cada final de carrera IR; `carro.c` tiene un timeout que limita el riesgo de empujar contra el tope.
- **Puerto serie**: `aplicacion/protocolo.c` implementa un protocolo de texto por USB CDC (comandos `MEDIR`, `CAL`, `REF`, `IR <mm>`… y eventos `EVT,…`; ver `protocolo.h`). No usa el SDK: `main.c` pasa a `app_ejecutar()` una función que lee un byte sin esperar (`getchar_timeout_us(0)`).
- **Medición de color**: la frecuencia del TCS3200 se cuenta por hardware con PWM2 B como contador de flancos, y la ventana de cada canal la gobierna una ISR del TIMER (`tcs3200_async`); la medición solo arranca cuando el IR de proximidad ve la muestra a la distancia correcta (300 ms continuos). Se promedian 3 lecturas y se aplica calibración negro/blanco antes de RGB → HSV → temperatura (`dominio/escala.c`): cálido (315°–75°), frío (75°–315°) o neutro (sin tono), cada una con una posición fija en el eje X (`ESCALA_POS_*_UM`).

## Pines y documentación generada

- Los pines se definen **solo** en `src/configuracion/pines.h` (con `_Static_assert` que detecta conflictos). GP23/24/25/29 están reservados por el chip Wi-Fi de la Pico W.
- `docs/conexiones.md` está **generado** por `tools/generar_conexiones.py` desde `pines.h`; no editarlo a mano, regenerar. `src/graficos/fuente5x7.c` sale de `tools/generar_fuente.py`.
- Todos los módulos van a **3V3 de una fuente externa con GND común**; el `VM` del TB6612FNG necesita 4,5–13,5 V aparte (en el banco, 5 V: el motor solo se mueve bien por encima del 90 % de ciclo de trabajo, `MOTOR_DUTY_MIN_PCT`).
- GP11, GP12 y GP13 están libres (el LED RGB se eliminó).

## Convenciones

- Código, comentarios, identificadores y salida por consola en español; archivos en minúsculas con guion bajo.
- Los módulos de `drivers/` y `dominio/` se prefijan con el nombre del módulo (`pin_*`, `tiempo_*`, `sistema_*`).
- Mensajes de commit: solo la descripción del cambio.
- El reparto por integrante está en `docs/reparto-trabajo.md`; las decisiones y hipótesis de hardware pendientes, en `docs/hardware.md`.
