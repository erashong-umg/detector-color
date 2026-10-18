"""Pines y parámetros del detector de color (Raspberry Pi Pico W, MicroPython).

Todo lo marcado HIPÓTESIS hay que confirmarlo en el banco (igual que en la
versión en C). Las posiciones se manejan en milímetros.
"""

# ---- Pines (GP) -------------------------------------------------------
# GP23/24/25/29 los usa el chip Wi-Fi de la Pico W: no usar.
TFT_DC, TFT_RST, TFT_SCK, TFT_MOSI = 16, 17, 18, 19   # ST7789 por SPI0 (sin CS)
TCS_S2, TCS_S3, TCS_OUT = 2, 3, 5                      # OUT debe ser impar (canal B del PWM)
IR_INICIO, IR_FIN = 4, 6                               # finales de carrera
IR_PROX_A, IR_PROX_D = 28, 1                           # IR de proximidad (analógico / digital)
MOTOR_STBY, MOTOR_PWM, MOTOR_IN1, MOTOR_IN2 = 7, 8, 9, 10   # TB6612FNG canal A
LED_R, LED_G, LED_B = 11, 12, 13                       # LED RGB de ánodo común
BUZZER = 15
BOTONES = (14, 20, 21, 0)                              # B1 medir, B2 calibrar, B3 mover, B4 guardar/menú
JOY_X, JOY_Y, JOY_SW = 26, 27, 22

# ---- Pantalla ---------------------------------------------------------
TFT_ANCHO = TFT_ALTO = 240
TFT_BAUDIOS = 62_500_000
TFT_INVERTIR = True          # panel IPS
TFT_MADCTL = 0x00            # orientación (0x00, 0x60, 0xC0, 0xA0)

# ---- Sensor de color --------------------------------------------------
TCS_VENTANA_US = 20_000      # ventana de conteo por canal
TCS_ESPERA_FILTRO_US = 2_000
MUESTRAS_COLOR = 3           # mediciones promediadas por lectura

# ---- Eje lineal y motor ----------------------------------------------
EJE_LONGITUD_MM = 320.0      # recorrido entre los dos finales
EJE_VEL_HOMING = 30          # % (1..100)
EJE_VEL_VIAJE = 40
EJE_TOLERANCIA_MM = 1.5
EJE_RECORRIDO_ESPERADO_MS = 8000      # HIPÓTESIS: medirlo en el banco
EJE_RECORRIDO_MIN_MS = 800            # menos que esto = sensores mal puestos
MOTOR_DUTY_MIN_PCT = 25               # el motor no arranca por debajo
MOTOR_RAMPA_MS = 3                    # ms por punto de velocidad
MOTOR_SIGNO = +1                      # -1 si con velocidad + va hacia el inicio
FINAL_ACTIVO_BAJO = True              # HIPÓTESIS: nivel bajo = carrito en el extremo
FINAL_ANTIREBOTE_MS = 5
CARRO_TIMEOUT_MS = 6000               # movimiento continuo máximo sin calibrar

# ---- Escala de color --------------------------------------------------
ESCALA_ANCHO_MM = 320.0      # gradiente de tono 0°..360° (rojo a rojo)
ESCALA_DESPLAZAMIENTO_MM = 0.0
ESCALA_BANDA_ROJO = 10       # ± grados donde 0° y 360° se consideran el mismo rojo
ESCALA_VAL_NEGRO = 40        # V por debajo: negro
ESCALA_SAT_MIN = 45          # S por debajo: sin color
ESCALA_VAL_BLANCO = 180      # sin color y V >= esto: blanco, si no gris
ESCALA_ZONA_MUERTA_MM = 3.0  # no se mueve si ya está así de cerca

# ---- IR de proximidad -------------------------------------------------
PROX_DIGITAL_ACTIVO_BAJO = True
PROX_ANALOGICO_INVERTIDO = True       # la tensión baja al acercarse el objeto
PROX_NIVEL_PRESENCIA = 400            # nivel 0..4095, mayor = más cerca
PROX_NIVEL_OK = 1500
PROX_NIVEL_CERCA = 3500
PROX_ESTABLE_MS = 300                 # distancia correcta sin interrupción antes de medir
ESPERA_OBJETO_MS = 8000               # se cancela si no se coloca la muestra

# ---- Entradas ---------------------------------------------------------
BOTON_ANTIREBOTE_MS = 20
BOTON_LARGO_MS = 800
JOY_ZONA_MUERTA = 12
JOY_INVERTIR_Y = True
