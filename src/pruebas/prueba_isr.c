/*
 * prueba_isr.c — Fase 2: adquisición del TCS3200 por interrupción frente a la bloqueante.
 *
 * Primero mide el bucle principal con tcs3200_medir() (espera ≈ 90 ms por vuelta) durante 3 s;
 * luego, con la adquisición por ISR (tcs3200_async), otros 3 s. Imprime por USB:
 *   - vueltas del bucle, vueltas por segundo y el mayor hueco entre dos vueltas;
 *   - juegos R-G-B-C obtenidos, interrupciones atendidas y ciclos de la ISR más larga;
 *   - el PC que tenía el programa cada vez que la ISR lo interrumpió (últimos 8).
 * Después sigue en bucle imprimiendo una línea por segundo.
 *
 * Evidencias que se sacan de aquí:
 *   - El bucle principal sigue vivo mientras se adquiere: pasa de ≈ 11 a millones de vueltas por segundo.
 *   - Los PC interrumpidos caen dentro de medir_lazo() (el bucle principal), no en la ISR.
 *       arm-none-eabi-addr2line -f -e build/detector_color.elf <PC>
 *   - Con un analizador lógico o un osciloscopio: GP11 (ISR) sube ≈ 12 veces por cada 88 ms
 *     mientras GP12 (bucle) conmuta sin parar.
 *   - Con el depurador: punto de interrupción en isr_irq0 y en medir_lazo; ambos se alcanzan.
 */
#include <inttypes.h>
#include <stdio.h>

#include "pruebas/pruebas.h"
#include "configuracion/pines.h"
#include "drivers/tcs3200.h"
#include "drivers/tcs3200_async.h"
#include "hw/ciclos.h"
#include "hw/pin.h"
#include "hw/tiempo.h"

#define DURACION_FASE_US  3000000u

typedef struct {
    uint32_t vueltas;
    uint32_t max_hueco_us;
    uint32_t juegos;
} resumen_t;

/* Bucle principal de la prueba. Cada vuelta conmuta GP12, lee el reloj y recoge una muestra
   si hay una disponible: es la "otra tarea" que no debe quedar bloqueada por la adquisición. */
__attribute__((noinline))
static resumen_t medir_lazo(bool con_isr, uint32_t duracion_us)
{
    resumen_t r = { 0, 0, 0 };
    tcs_frecuencias_t f;
    bool valido;
    uint32_t inicio = tiempo_us();
    uint32_t previo = inicio;

    for (;;) {
        uint32_t ahora = tiempo_us();
        if ((uint32_t)(ahora - inicio) >= duracion_us) {
            break;
        }
        uint32_t hueco = ahora - previo;
        if (hueco > r.max_hueco_us) {
            r.max_hueco_us = hueco;
        }
        previo = ahora;

        pin_alternar(PIN_DEP_LAZO);
        r.vueltas++;

        if (con_isr) {
            if (tcs3200_async_tomar(&f, &valido)) {
                r.juegos++;
            }
        } else {
            tcs3200_medir(&f);              /* bloquea ≈ 90 ms */
            r.juegos++;
        }
    }
    return r;
}

static void informe(const char *titulo, const resumen_t *r)
{
    uint32_t por_s = (uint32_t)((uint64_t)r->vueltas * 1000000u / DURACION_FASE_US);
    printf("%s\n", titulo);
    printf("  vueltas del bucle: %" PRIu32 "  (%" PRIu32 " por segundo)\n", r->vueltas, por_s);
    printf("  mayor hueco entre vueltas: %" PRIu32 " us\n", r->max_hueco_us);
    printf("  juegos R-G-B-C obtenidos: %" PRIu32 "\n", r->juegos);
}

void prueba_isr(void)
{
    printf("\n== Fase 2: adquisicion por interrupcion ==\n");
    printf("Pines de depuracion: GP%u = ISR, GP%u = bucle principal\n",
           (unsigned)PIN_DEP_ISR, (unsigned)PIN_DEP_LAZO);

    ciclos_iniciar();                       /* para medir la duracion de la ISR */
    pin_salida(PIN_DEP_LAZO, false);
    tcs3200_iniciar();

    resumen_t a = medir_lazo(false, DURACION_FASE_US);
    informe("A) Medicion bloqueante (tcs3200_medir)", &a);

    tcs3200_async_iniciar();
    resumen_t b = medir_lazo(true, DURACION_FASE_US);
    informe("B) Adquisicion por ISR (tcs3200_async)", &b);

    printf("  interrupciones atendidas: %" PRIu32 "\n", tcs3200_async_interrupciones());
    printf("  ISR mas larga: %" PRIu32 " ciclos\n", tcs3200_async_isr_ciclos_max());
    printf("  PC interrumpidos (resolver con addr2line):");
    for (uint32_t i = 0; i < 8; i++) {
        printf(" 0x%08" PRIx32, tcs3200_async_pc_interrumpido(i));
    }
    printf("\n");
    if (a.vueltas != 0) {
        printf("Mejora del bucle: x%" PRIu32 " vueltas en el mismo tiempo\n", b.vueltas / a.vueltas);
    }

    /* Seguir: una linea por segundo con la ultima muestra. El bucle no se detiene. */
    tcs_frecuencias_t f = { 0, 0, 0, 0 };
    bool valido = true;
    uint32_t vueltas = 0;
    uint32_t ultimo = tiempo_us();
    for (;;) {
        pin_alternar(PIN_DEP_LAZO);
        vueltas++;
        tcs3200_async_tomar(&f, &valido);

        if ((uint32_t)(tiempo_us() - ultimo) >= 1000000u) {
            ultimo += 1000000u;
            printf("vueltas/s=%" PRIu32 " juegos=%" PRIu32 " R=%" PRIu32 " G=%" PRIu32 " B=%" PRIu32
                   " C=%" PRIu32 " Hz%s\n",
                   vueltas, tcs3200_async_juegos(), f.r, f.g, f.b, f.c, valido ? "" : " (desborde)");
            vueltas = 0;
        }
    }
}
