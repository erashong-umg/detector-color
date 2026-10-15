#include "drivers/led.h"

#include "configuracion/actuadores.h"
#include "configuracion/pines.h"
#include "hw/pin.h"
#include "hw/rp2040_regs.h"

#define SL_R   RP_PWM_SLICE(PIN_LED_R)
#define SL_G   RP_PWM_SLICE(PIN_LED_G)
#define SL_B   RP_PWM_SLICE(PIN_LED_B)

/* Cada canal: slice y si va en el canal B (bits 31:16 de CC) o en el A (15:0). */
typedef struct {
    uint8_t  pin;
    uint8_t  slice;
    uint8_t  desp;          /* 0 o 16 */
    uint32_t escala;
} canal_led_t;

static const canal_led_t canales[3] = {
    { PIN_LED_R, SL_R, RP_PWM_CANAL_B(PIN_LED_R) ? 16 : 0, LED_ESCALA_R },
    { PIN_LED_G, SL_G, RP_PWM_CANAL_B(PIN_LED_G) ? 16 : 0, LED_ESCALA_G },
    { PIN_LED_B, SL_B, RP_PWM_CANAL_B(PIN_LED_B) ? 16 : 0, LED_ESCALA_B },
};

/* Gamma 2,0 sobre 0..255, con la escala del canal aplicada antes. */
static uint32_t nivel(uint32_t valor, uint32_t escala)
{
    uint32_t v = valor * escala / 255u;
    uint32_t g = v * v / 255u;
    /* TOP = 255 cuenta 0..255: un CC de 256 (> TOP) deja la salida activa
       todo el periodo, es el único modo de llegar al 100 %. */
    return g == 255u ? 256u : g;
}

static void escribir(int i, uint32_t valor)
{
    const canal_led_t *c = &canales[i];
    volatile uint32_t *cc = &RP_PWM->slice[c->slice].cc;
    *cc = (*cc & ~(0xFFFFu << c->desp)) | (nivel(valor, c->escala) << c->desp);
}

void led_iniciar(void)
{
    rp_liberar_reset(RP_RESET_PWM);

    uint32_t usadas = (1u << SL_R) | (1u << SL_G) | (1u << SL_B);
    for (uint32_t sl = 0; sl < 8; sl++) {
        if (!(usadas & (1u << sl))) {
            continue;
        }
        rp_pwm_slice_t *s = &RP_PWM->slice[sl];
        s->csr = 0;
        s->div = RP_PWM_DIV(LED_DIV_ENTERO, 0);
        s->top = LED_TOP;
        s->ctr = 0;
        s->cc  = 0;
    }

    /* Ánodo común: polaridad invertida en cada canal usado. */
    for (int i = 0; i < 3; i++) {
        uint32_t inv = canales[i].desp ? RP_PWM_CSR_B_INV : RP_PWM_CSR_A_INV;
        RP_PWM_SET->slice[canales[i].slice].csr = inv;
    }
    for (uint32_t sl = 0; sl < 8; sl++) {
        if (usadas & (1u << sl)) {
            RP_PWM_SET->slice[sl].csr = RP_PWM_CSR_EN;
        }
    }

    for (int i = 0; i < 3; i++) {
        pin_funcion(canales[i].pin, RP_FUNC_PWM);
    }
}

void led_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    escribir(0, r);
    escribir(1, g);
    escribir(2, b);
}

void led_color(color_rgb_t c)
{
    led_rgb(c.r, c.g, c.b);
}

void led_apagar(void)
{
    led_rgb(0, 0, 0);
}
