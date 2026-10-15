# Reparto del trabajo por temas — Grupo 1

El reparto es por **tema del proyecto** (hardware y drivers primero), no por cantidad de líneas.
La carga técnica mayor recae en Erick; después Werner, y el resto en el orden de la tabla.

| # | Integrante | Carné | Tema |
|---|---|---|---|
| 1 | Erick Orlando Rashon Gonzalez | 9490-11-5609 | Núcleo del RP2040, sensor de color, motor e integración |
| 2 | Werner Josue Ortiz Abdalla | 9040-20-1143 | Pantalla ST7789 (SPI), gráficos e interfaz |
| 3 | Brayan Kenet Rivera Quinilla | 9490-23-2835 | Entradas analógicas (ADC) y algoritmo de color |
| 4 | Wilder Alfonso Castillo Salguero | 9490-21-3708 | Riel lineal: finales de carrera, homing y posición |
| 5 | Marco Tulio Pineda Recinos | 9490-23-2906 | Botones, LED RGB, buzzer y documentación |

## 1. Erick — núcleo del RP2040, TCS3200, motor e integración

- **Acceso al hardware por structs de registros** (`src/hw/`): GPIO, IO_BANK0, PADS, RESETS, relojes
  y temporización. Es la base de la que dependen todos los demás drivers.
- **Sensor TCS3200:** selección de filtro y escala de frecuencia; medición de la frecuencia con
  PWM2 B como contador de flancos por hardware.
- **Motor TB6612FNG:** dirección y velocidad por PWM, freno y punto muerto.
- **Máquina de estados de la aplicación** (referenciar → medir → mapear → mover) e integración de
  todos los módulos.
- Arranque, CMake, asignación de pines (`pines.h`) y pruebas en PC (`test/host/`).
- Revisión final del código de los demás.

## 2. Werner — pantalla ST7789 e interfaz

- **Driver ST7789 240×240 por SPI0** (modo 3, sin CS): secuencia de inicialización, ventana de
  dibujo y volcado de píxeles.
- **Gráficos:** fuente 5×7, texto, rectángulos y colores en RGB565.
- **Interfaz de usuario:** pantallas, menú y navegación.

## 3. Brayan — ADC y color

- **ADC:** lectura del joystick y del IR de proximidad/distancia de la muestra.
- **Algoritmo de color:** calibración negro/blanco, RGB → HSV, tono → posición en la escala impresa
  y manejo de acromáticos y de la banda del rojo.

## 4. Wilder — riel lineal

- **Finales de carrera IR** (inicio y fin): lectura y filtrado de rebotes.
- **Homing y recorrido:** búsqueda del inicio y medición de la longitud útil.
- **Posición por tiempo:** estimación de la posición del carrito a partir de la velocidad y el
  tiempo, y límites de seguridad.

## 5. Marco — entradas/salidas de usuario y documentación

- **Botones (4):** lectura, antirrebote y eventos de pulsación corta/larga.
- **LED RGB (ánodo común):** control por PWM con lógica invertida.
- **Buzzer pasivo:** generación de tonos y melodías de realimentación.
- **Documentación:** README, `docs/`, diagrama de conexiones y manual de compilación/pruebas.

## Reglas comunes

- Acceso a periféricos solo con los structs `rp_*`; **no** usar la HAL del SDK.
- `dominio/` y `aplicacion/` no incluyen cabeceras del SDK.
- Código, comentarios e identificadores en español.
- Cada persona hace commits de su parte, con mensajes que describan solo el cambio.
