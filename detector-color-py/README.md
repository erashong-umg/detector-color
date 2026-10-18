# Detector de color — versión MicroPython

Misma idea que el firmware en C (`../src`), pero en MicroPython y mucho más corta: mide el
color con un TCS3200, lo pasa a tono (HSV) y mueve el carrito hasta su posición en la escala
impresa, mostrando todo en la ST7789. Conexiones, hardware y reparto del trabajo: ver `../docs/`
(los pines son los mismos; están en `config.py`).

## Archivos

| Archivo | Qué hace |
|---|---|
| `main.py` | arranca la aplicación |
| `config.py` | pines y parámetros (las HIPÓTESIS por medir en el banco están aquí) |
| `app.py` | máquina de estados, órdenes, LED/buzzer y bucle principal |
| `carro.py` | motor + finales: homing, calibración, posición por tiempo, seguridad |
| `sensores.py` | TCS3200, IR de proximidad, finales, botones y joystick |
| `actuadores.py` | motor TB6612FNG, LED RGB y buzzer |
| `color.py` | calibración, RGB→HSV y color→posición (sin hardware) |
| `pantalla.py`, `ui.py`, `fuente.py` | ST7789, pantallas y fuente 5×7 |
| `diagnostico.py` | pruebas de banco desde el REPL (no las carga `main.py`) |
| `test_pc.py` | pruebas en el PC con hardware simulado (no se sube a la Pico) |

## Abrir como proyecto de Pico (VS Code)

La carpeta ya está configurada para la extensión **MicroPico** (`paulober.pico-w-go`, la que usa
*Raspberry Pi Pico* para los proyectos MicroPython): tiene `.micropico` y `.vscode/` con los stubs.

1. En VS Code: *File > Open Folder…* y abrir **`detector-color-py`** (no la carpeta padre: el `.micropico`
   tiene que estar en la raíz del espacio de trabajo).
2. Conectar la Pico W por USB con MicroPython cargado; la extensión se conecta sola (`micropico.autoConnect`).
   Si no, botón *Pico Connect* de la barra de estado o `MicroPico: Connect`.
3. Subir el proyecto: `MicroPico: Upload project to Pico` (no sube `test_pc.py`, `README.md` ni `.vscode`; ver
   `micropico.pyIgnore`). Reiniciar con `MicroPico: Soft reset` y `main.py` arranca solo.
4. Probar un archivo suelto: abrirlo y `MicroPico: Run current file on Pico`; por ejemplo `diagnostico.py`
   y luego, en el REPL de la extensión, `import diagnostico; diagnostico.finales()`.
5. Si no hay MicroPython en la placa: `MicroPico: Flash Pico` o cargar el `.uf2` a mano (ver abajo).

## Instalar en la Pico W (sin la extensión)

1. Cargar el MicroPython de la Pico W (`.uf2` de micropython.org, *Raspberry Pi Pico W*, v1.22 o
   más nuevo) manteniendo BOOTSEL al conectar.
2. Subir los archivos con [`mpremote`](https://docs.micropython.org/en/latest/reference/mpremote.html)
   (`pip install mpremote`) o con Thonny:

```powershell
cd detector-color-py
mpremote cp main.py app.py carro.py sensores.py actuadores.py color.py pantalla.py ui.py fuente.py config.py diagnostico.py :
mpremote reset
```

Para ver los mensajes o probar piezas sueltas: `mpremote repl` y
`import diagnostico; diagnostico.finales()` (también `entradas`, `proximidad`, `color_crudo`, `motor`).
Si falla algo al arrancar, el error sale en el REPL.

## Puesta en marcha (lo que sigue siendo hipótesis)

Nada de esto se ha probado en el banco todavía. Con `diagnostico.py` hay que confirmar, en orden:
polaridad de los finales (`FINAL_ACTIVO_BAJO`), sentido del motor (`MOTOR_SIGNO`), niveles y polaridad
del IR de proximidad (`PROX_*`), recorrido real (`EJE_LONGITUD_MM`, `EJE_RECORRIDO_ESPERADO_MS`) y
dónde empieza y acaba la escala impresa (`ESCALA_*`).

## Pruebas en el PC

```bash
python test_pc.py
```

Ejecuta el mismo código contra un `machine`, un `framebuf` y un reloj simulados, con un carrito
físico ideal: color y escala, homing desde tres posiciones, error de posición en 6 destinos, fallos
(timeout, sentido invertido, finales incoherentes) y la aplicación completa (calibrar, medir, mover,
guardar, navegar todas las pantallas). Valida la lógica, no el cableado.

## Qué cambia respecto a la versión en C

- **Usa la API `machine`** (`Pin`, `PWM`, `ADC`, `SPI`) en vez de structs de registros. Solo el contador
  de flancos del TCS3200 toca registros (`mem32`), porque `machine.PWM` no sabe contar entradas.
  Por eso esta versión **no cumple la regla del curso «sin HAL»**; si la entrega la exige, la versión
  válida es la de C.
- Una sola pila de módulos en vez de capas hexagonales; posiciones en mm (float) en vez de µm.
- Se quitaron: la «referencia previa» por el final más cercano, la velocidad de aproximación (era igual
  a la de viaje) y los 8 programas de prueba (los sustituye `diagnostico.py`).
- Pendiente igual que en C: guardar calibración e historial en flash, y el envío por Bluetooth.
