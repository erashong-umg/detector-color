# Pruebas en el PC (sin la Pico)

Compilan y ejecutan en Windows el **mismo código** que va en el firmware, sustituyendo
solo lo que toca hardware por simulaciones:

| Simulación | Sustituye a | Qué hace |
|---|---|---|
| `mock/hw/tiempo.h` | `hw/tiempo.h` | el reloj es una variable que avanza al "esperar" |
| `mock_st7789.c` | `drivers/st7789.c` | escribe en un framebuffer 240×240 y lo vuelca a PPM |
| `mock_motor.c` | `drivers/motor.c` | misma rampa y mapeo de velocidad; alimenta a la planta |
| `mock_finales.c` | `drivers/finales.c` | finales según la posición del carrito, con antirrebote de 5 ms |
| `mock_adc.c`, `mock/hw/pin.h` | `drivers/adc.c`, `hw/pin.h` | el canal 2 del ADC y el pin OD del IR de proximidad devuelven lo que fija la prueba |
| `mock_tcs3200.c` | `drivers/tcs3200.c` | frecuencias programables; gasta el mismo tiempo que la medición real |

## Uso

```bash
test/host/compilar.sh                       # usa D:\dev\w64devkit (gcc para Windows)
RUIDO=0.05 test/host/compilar.sh            # variación de velocidad del motor ±5 %
```

- **Interfaz** (`harness_ui.c`): dibuja las 10 pantallas y las deja en
  `test/host/build/ui/` (`.ppm` y `montaje_*.png`). Sirve para ver textos
  cortados, solapados o fuera de pantalla antes de cargar el firmware. Cuenta los
  bloques que habría descartado el driver real por salirse de la pantalla.
- **Eje** (`harness_eje.c`): ejecuta `eje.c`, `carro.c`, `posicion.c` y `limites.c`
  contra una planta física con zona muerta y ruido de velocidad: homing desde tres
  posiciones, fallos (sentido invertido, finales estropeados, recorrido más lento
  que el esperado), error de posición en 10 destinos y repetibilidad en 8 viajes.

- **Máquina de estados** (`test_sistema.c`): 86 comprobaciones con el código real de
  `sistema.c`, `eje.c`, `carro.c`, `deteccion.c` y del IR de proximidad: clasificación con
  histéresis, órdenes rechazadas, homing, espera y guía del objeto, medir y llevar el cursor,
  colores sin tono, lectura inválida, cancelar, historial, modo manual, desarmar en plena
  operación, errores con recuperación y arranque con los dos finales activos. Devuelve un
  código de salida distinto de cero si algo falla.

## Límites

La planta es un modelo: no incluye inercia al frenar, holgura de la correa ni
deslizamiento. Los números son una referencia de comportamiento, no una garantía;
la exactitud real solo se mide con el carrito y una regla.

Compilar para la Pico sigue siendo con `cmake`/`ninja` desde la carpeta del proyecto.
