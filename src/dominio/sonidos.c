#include "dominio/sonidos.h"

#define N(a) ((uint8_t)(sizeof(a) / sizeof((a)[0])))

/* Notas (Hz): C5 523, E5 659, G5 784, C6 1047, E6 1319, G6 1568 */
static const nota_t inicio[]    = { {523, 90}, {659, 90}, {784, 90}, {1047, 180} };
static const nota_t clic[]      = { {2000, 15} };
static const nota_t confirmar[] = { {1047, 80}, {1319, 80}, {1568, 160} };
static const nota_t alerta[]    = { {880, 120}, {0, 80}, {880, 120}, {0, 80}, {880, 120} };
static const nota_t error[]     = { {400, 200}, {0, 40}, {300, 350} };

const sonido_t SONIDO_INICIO    = { inicio,    N(inicio)    };
const sonido_t SONIDO_CLIC      = { clic,      N(clic)      };
const sonido_t SONIDO_CONFIRMAR = { confirmar, N(confirmar) };
const sonido_t SONIDO_ALERTA    = { alerta,    N(alerta)    };
const sonido_t SONIDO_ERROR     = { error,     N(error)     };
