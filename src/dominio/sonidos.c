#include "dominio/sonidos.h"

#define N(a) ((uint8_t)(sizeof(a) / sizeof((a)[0])))

/* Sonidos de control numérico (CNC): tonos puros y cortos, como los de un panel de máquina.
   Frecuencias de 1,5 a 3,5 kHz, cerca de la resonancia de un zumbador pasivo.
     clic         tic agudo y muy corto          pulsación de botón (pendant)
     confirmar    2 pitidos iguales              orden aceptada / ciclo correcto
     inicio       3 notas ascendentes            «servo on»: sistema listo
     moviendo     rampa ascendente de eje        el carrito arranca (aceleración)
     llegada      tic doble, el 2.º más agudo    carrito «en posición»
     home         3 notas ascendentes y largas   referencia del eje terminada («home done»)
     analizando   barrido de sonda               midiendo la muestra (≈ 270 ms), como un escáner
     alerta       2 pitidos lentos               aviso: acerque o aleje la muestra, reintento
     error        sirena alterna agudo-grave     alarma: fallo, parada segura */
static const nota_t clic[]      = { {3500, 12} };
static const nota_t confirmar[] = { {2700, 60}, {0, 50}, {2700, 60} };
static const nota_t inicio[]    = { {1500, 70}, {0, 20}, {2000, 70}, {0, 20}, {2700, 110} };
static const nota_t moviendo[]  = { {500, 25}, {700, 25}, {900, 25}, {1100, 25}, {1300, 25}, {1500, 25}, {1700, 25}, {1900, 25} };
static const nota_t llegada[]   = { {2400, 40}, {0, 30}, {3200, 70} };
static const nota_t home[]      = { {1800, 80}, {0, 30}, {2400, 80}, {0, 30}, {3000, 160} };
static const nota_t alerta[]    = { {2000, 180}, {0, 180}, {2000, 180} };

/* Barrido de sonda: sube y baja de tono en escalones de 18 ms (≈ 270 ms, lo que tarda
   la medición). Durante la medición se llama al buzzer desde la espera del sensor
   (deteccion_poner_espera), así que las notas avanzan aunque el bucle esté bloqueado. */
static const nota_t analizando[] = {
    {1000, 18}, {1250, 18}, {1500, 18}, {1800, 18}, {2100, 18}, {2450, 18}, {2800, 18}, {3200, 18},
    {3200, 18}, {2800, 18}, {2450, 18}, {2100, 18}, {1800, 18}, {1500, 18}, {1250, 18}, {1000, 18}
};

static const nota_t error[] = {
    {3000, 110}, {1500, 110}, {3000, 110}, {1500, 110}, {3000, 110}, {1500, 110}, {3000, 110}, {1500, 110}
};

const sonido_t SONIDO_INICIO     = { inicio,     N(inicio)     };
const sonido_t SONIDO_CLIC       = { clic,       N(clic)       };
const sonido_t SONIDO_CONFIRMAR  = { confirmar,  N(confirmar)  };
const sonido_t SONIDO_MOVIENDO   = { moviendo,   N(moviendo)   };
const sonido_t SONIDO_LLEGADA    = { llegada,    N(llegada)    };
const sonido_t SONIDO_HOME       = { home,       N(home)       };
const sonido_t SONIDO_ANALIZANDO = { analizando, N(analizando) };
const sonido_t SONIDO_ALERTA     = { alerta,     N(alerta)     };
const sonido_t SONIDO_ERROR      = { error,      N(error)      };
