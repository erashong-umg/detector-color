# Compilar, cargar y depurar

La configuración es **la que genera la extensión *Raspberry Pi Pico* (0.23.0)**: se obtuvo
ejecutando su propia función `generateProjectFiles()` y copiando el resultado, no a mano.
Si la extensión cambia de versión, conviene regenerarla igual (ver el final).

## Cómo está montado

| Pieza | Valor |
|---|---|
| SDK / toolchain / picotool | 2.3.1 / `15_2_Rel1` / 2.3.1 (de `C:\Users\Erick\.pico-sdk`) |
| CMake / Ninja / OpenOCD | v4.3.4 / v1.13.2 / 0.12.0+dev (de `.pico-sdk`) |
| Placa | `pico_w` (RP2040) |
| Tipo de compilación | `Debug` (así configura la extensión; el SDK usaría Release sin él) |
| Sonda | DebugProbe / CMSIS-DAP (`interface/cmsis-dap.cfg`) |

- `CMakeLists.txt` lleva el bloque `# == DO NOT EDIT ... ==`, que incluye
  `${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake` y fija las rutas del SDK y del
  toolchain. **No tocar ese bloque**: la extensión lo regenera.
- `.vscode/` son los seis archivos de la extensión: `settings.json`, `launch.json`,
  `tasks.json`, `c_cpp_properties.json`, `cmake-kits.json` y `extensions.json`.
  Las rutas usan `${userHome}`, `${env:USERPROFILE}` y `${command:raspberry-pi-pico.*}`.
- Las variables de usuario de Windows `PICO_SDK_PATH` / `PICO_TOOLCHAIN_PATH` (hoy apuntan
  a `D:\dev\pico-sdk`, SDK 2.3.0) **no afectan**: el bloque de CMake fija la variable antes
  de que el SDK mire el entorno. Comprobado con un build desde cero.

## Desde VS Code

| Acción | Cómo |
|---|---|
| Configurar | automático al abrir (`cmakeAutoConfigure`) |
| Compilar | tarea **Compile Project** (`Ctrl+Shift+B`) |
| Cargar por USB | tarea **Run Project** (`picotool load … -fx`, con la Pico en BOOTSEL) |
| Cargar por sonda | tarea **Flash** (OpenOCD) |
| Depurar | **Pico Debug (Cortex-Debug)**, F5; para en `main` |
| Rescatar una placa colgada | tarea **Rescue Reset** |

> **Cuidado con *Run Project* y *Flash*:** sobrescriben lo que haya en la Pico.
> Para ver otra prueba, cambiar `PRUEBA_ACTIVA` en `src/main.c` y recompilar.

## Desde un terminal (equivale a lo que hace la extensión)

En el terminal integrado de VS Code el `PATH` ya está preparado por `settings.json`.
En un terminal externo hay que usar las rutas de `.pico-sdk`:

```powershell
$p = "$env:USERPROFILE\.pico-sdk"
$env:Path = "$p\toolchain\15_2_Rel1\bin;$p\picotool\2.3.1\picotool;$p\cmake\v4.3.4\bin;$p\ninja\v1.13.2;$env:Path"
cmake -G Ninja -B build . -DCMAKE_BUILD_TYPE=Debug     # configurar
ninja -C build                                          # compilar
picotool load build/detector_color.uf2 -fx              # cargar (BOOTSEL)
```

**Git Bash:** el `PATH` resuelve `arm-none-eabi-gcc` a un GCC 4.7 antiguo
(`C:\yagarto-20121222`), que no sirve. Compilar desde el terminal de VS Code o con
las rutas completas de arriba.

## Qué se ha verificado y qué no

- **Verificado (sin sonda):** configuración y compilación desde cero con cero avisos;
  el `.elf` lleva símbolos; GDB resuelve `main` (`src/main.c:26`); OpenOCD lee las
  configuraciones de CMSIS-DAP y del RP2040 y se detiene en
  *"unable to find a matching CMSIS-DAP device"*, que es lo esperado sin sonda.
- **Sin verificar:** una sesión de depuración real. Hace falta una sonda CMSIS-DAP
  (otra Pico con `debugprobe`) cableada a SWCLK/SWDIO/GND de la Pico W.

## Si la sonda es un ST-Link

La extensión **solo genera** configuraciones para CMSIS-DAP y para SWD desde una
Raspberry Pi. El proyecto *Lunar Lander* tiene además variantes para ST-Link
(`.vscode/openocd/rp2040-stlink.cfg`, `Pico Debug (ST-Link)`, `Flash (ST-Link)`), que son
personalizaciones propias y **no se han traído aquí**. Si la sonda de este proyecto es
un ST-Link, hay que añadirlas a mano (y la extensión las borrará al regenerar).

## Regenerar `.vscode/` con el generador de la extensión

El generador está en `%USERPROFILE%\.vscode\extensions\raspberry-pi.raspberry-pi-pico-<versión>\scripts\pico_project.py`.
Se llama con `generateProjectFiles(...)` (proyecto, sonda, versiones y rutas) sobre una
carpeta temporal, y se copia el contenido de su `.vscode/` aquí. No generar
directamente sobre el proyecto: pisaría `.gitignore` y `extensions.json`.
