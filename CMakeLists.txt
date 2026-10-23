# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(detector_color C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(detector_color
    src/main.c
    src/hw/pin.c
    src/drivers/st7789.c
    src/graficos/fuente5x7.c
    src/graficos/texto.c
    src/ui/interfaz.c
    src/drivers/tcs3200.c
    src/drivers/adc.c
    src/drivers/botones.c
    src/drivers/joystick.c
    src/drivers/buzzer.c
    src/drivers/motor.c
    src/drivers/finales.c
    src/aplicacion/carro.c
    src/aplicacion/eje.c
    src/aplicacion/deteccion.c
    src/aplicacion/sistema.c
    src/aplicacion/app.c
    src/aplicacion/protocolo.c
    src/drivers/proximidad.c
    src/dominio/color.c
    src/dominio/eje.c
    src/dominio/sonidos.c
    src/dominio/limites.c
    src/dominio/posicion.c
    src/dominio/escala.c
    src/dominio/selector.c
    src/dominio/historial.c
    src/dominio/proximidad.c
    src/pruebas/prueba_pantalla.c
    src/pruebas/prueba_tcs3200.c
    src/pruebas/prueba_entradas.c
    src/pruebas/prueba_actuadores.c
    src/pruebas/prueba_motor.c
    src/pruebas/prueba_eje.c
    src/pruebas/prueba_mapeo.c
    src/pruebas/prueba_proximidad.c
    src/pruebas/prueba_isr.c
    src/pruebas/prueba_ciclos.c
    src/drivers/tcs3200_async.c
    src/dominio/color_rapido.c
    src/dominio/color_ref_float.c
)

target_include_directories(detector_color PRIVATE src)
target_compile_options(detector_color PRIVATE -Wall -Wextra)

# Del SDK solo se usa pico_stdlib: arranque (boot2, crt0), relojes y printf
# por USB. GPIO, SPI, PWM, ADC y TIMER se manejan con los struct de src/hw/.
target_link_libraries(detector_color
    pico_stdlib
)

pico_enable_stdio_usb(detector_color 1)
pico_enable_stdio_uart(detector_color 0)
pico_add_extra_outputs(detector_color)
