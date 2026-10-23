/*
 * prueba_ciclos.c — Fase 3: ciclos, CPI y speedup de la cadena normalizar → RGB → HSV.
 *
 * Tres versiones sobre el MISMO bloque de N muestras:
 *   A  float            dominio/color_ref_float.c   (referencia: float emulado por software)
 *   B  entera actual    dominio/color.c             (divisiones enteras, 64 bits al normalizar)
 *   C  optimizada       dominio/color_rapido.c      (punto fijo, recíprocos precalculados)
 *
 * Cada bloque se mide con SysTick (hw/ciclos.h) con las interrupciones enmascaradas, tras una
 * pasada de calentamiento (la flash se ejecuta por XIP con caché), REPS veces; se descuenta
 * el costo de llamar a un bloque vacío. Imprime por USB cada 5 s.
 *
 * Para el CPI hace falta el número de instrucciones: bench_una_a/b/c procesan UNA muestra fija y
 * se pueden contar paso a paso con tools/contar_instrucciones.gdb. CPI = ciclos / instrucciones
 * de esa misma muestra.
 */
#include <inttypes.h>
#include <stdio.h>

#include "pruebas/pruebas.h"
#include "dominio/color.h"
#include "dominio/color_rapido.h"
#include "dominio/color_ref_float.h"
#include "hw/ciclos.h"
#include "hw/tiempo.h"

#define N      256u
#define REPS   10u

static const color_calibracion_t cal = {
    .negro  = { 3200, 3500, 4100, 3000 },
    .blanco = { 82000, 91000, 76000, 200000 },
    .hay_negro = true,
    .hay_blanco = true,
};
static color_rapido_cal_t cal_rapida;

static color_frecuencias_t muestras[N];
static color_hsv_t sal_a[N], sal_b[N], sal_c[N];

/* Muestra única para contar instrucciones con el depurador. */
static const color_frecuencias_t UNA = { 41000, 52000, 30000, 90000 };
color_hsv_t sal_una;

static uint32_t semilla = 2463534242u;
static uint32_t aleatorio(void)
{
    semilla ^= semilla << 13;
    semilla ^= semilla >> 17;
    semilla ^= semilla << 5;
    return semilla;
}

/* Frecuencias repartidas entre un poco por debajo del negro y un poco por encima del blanco. */
static void generar_muestras(void)
{
    for (uint32_t i = 0; i < N; i++) {
        muestras[i].r = cal.negro.r - 300u + aleatorio() % (cal.blanco.r - cal.negro.r + 2000u);
        muestras[i].g = cal.negro.g - 300u + aleatorio() % (cal.blanco.g - cal.negro.g + 2000u);
        muestras[i].b = cal.negro.b - 300u + aleatorio() % (cal.blanco.b - cal.negro.b + 2000u);
        muestras[i].c = cal.blanco.r;
    }
}

__attribute__((noinline)) static void bloque_vacio(void) { __asm__ volatile (""); }

__attribute__((noinline)) static void bloque_a(void)
{
    for (uint32_t i = 0; i < N; i++) {
        sal_a[i] = color_flotante_procesar(&cal, &muestras[i]);
    }
}

__attribute__((noinline)) static void bloque_b(void)
{
    for (uint32_t i = 0; i < N; i++) {
        color_rgb_t rgb;
        color_normalizar(&cal, &muestras[i], &rgb);
        sal_b[i] = color_a_hsv(rgb);
    }
}

__attribute__((noinline)) static void bloque_c(void)
{
    for (uint32_t i = 0; i < N; i++) {
        sal_c[i] = color_rapido_procesar(&cal_rapida, &muestras[i]);
    }
}

__attribute__((noinline, used)) void bench_una_a(void) { sal_una = color_flotante_procesar(&cal, &UNA); }
__attribute__((noinline, used)) void bench_una_b(void)
{
    color_rgb_t rgb;
    color_normalizar(&cal, &UNA, &rgb);
    sal_una = color_a_hsv(rgb);
}
__attribute__((noinline, used)) void bench_una_c(void) { sal_una = color_rapido_procesar(&cal_rapida, &UNA); }

typedef struct {
    uint32_t minimo;
    uint32_t promedio;
    bool desbordo;
} medida_t;

static medida_t medir(void (*bloque)(void), uint32_t sobrecoste)
{
    medida_t m = { 0xFFFFFFFFu, 0, false };
    uint64_t suma = 0;

    bloque();                                   /* calienta la caché de la flash */
    for (uint32_t rep = 0; rep < REPS; rep++) {
        uint32_t previo = ciclos_irq_apagar();
        (void)ciclos_desbordo();                /* borra COUNTFLAG */
        uint32_t c0 = ciclos_leer();
        bloque();
        uint32_t c1 = ciclos_leer();
        if (ciclos_desbordo()) {
            m.desbordo = true;
        }
        ciclos_irq_restaurar(previo);

        uint32_t d = ciclos_entre(c0, c1);
        d = d > sobrecoste ? d - sobrecoste : 0;
        if (d < m.minimo) {
            m.minimo = d;
        }
        suma += d;
    }
    m.promedio = (uint32_t)(suma / REPS);
    return m;
}

static uint32_t medir_cpu_khz(void)
{
    (void)ciclos_desbordo();
    uint32_t c0 = ciclos_leer();
    tiempo_esperar_us(10000u);
    uint32_t c1 = ciclos_leer();
    return ciclos_entre(c0, c1) / 10u;          /* ciclos en 10 ms / 10 = kHz */
}

static void cociente_x100(const char *nombre, uint32_t num, uint32_t den)
{
    uint32_t x = den ? (uint32_t)((uint64_t)num * 100u / den) : 0;
    printf("  Speedup %s = %" PRIu32 ".%02" PRIu32 "\n", nombre, x / 100u, x % 100u);
}

static void fila(const char *nombre, const medida_t *m, uint32_t khz)
{
    /* us por bloque = ciclos / (kHz / 1000) = ciclos · 1000 / kHz */
    uint32_t us_x10 = (uint32_t)((uint64_t)m->minimo * 10000u / khz);
    printf("  %-18s min=%7" PRIu32 " prom=%7" PRIu32 " ciclos/bloque  %5" PRIu32 " ciclos/muestra  %4" PRIu32 ".%" PRIu32
           " us/bloque%s\n",
           nombre, m->minimo, m->promedio, m->minimo / N, us_x10 / 10u, us_x10 % 10u,
           m->desbordo ? "  (SysTick desbordo: invalido)" : "");
}

/* Ciclos de UNA muestra (la más corta de REPS) para dividir entre las instrucciones contadas. */
static uint32_t ciclos_una(void (*f)(void), uint32_t sobrecoste)
{
    uint32_t mejor = 0xFFFFFFFFu;
    f();
    for (uint32_t i = 0; i < REPS; i++) {
        uint32_t previo = ciclos_irq_apagar();
        uint32_t c0 = ciclos_leer();
        f();
        uint32_t c1 = ciclos_leer();
        ciclos_irq_restaurar(previo);
        uint32_t d = ciclos_entre(c0, c1);
        d = d > sobrecoste ? d - sobrecoste : 0;
        if (d < mejor) {
            mejor = d;
        }
    }
    return mejor;
}

static int abs_i(int x) { return x < 0 ? -x : x; }

static void informe(void)
{
    uint32_t khz = medir_cpu_khz();
    medida_t vacio = medir(bloque_vacio, 0);
    uint32_t sobrecoste = vacio.minimo;

    medida_t a = medir(bloque_a, sobrecoste);
    medida_t b = medir(bloque_b, sobrecoste);
    medida_t c = medir(bloque_c, sobrecoste);

    printf("\n== Fase 3: ciclos de la cadena normalizar -> HSV ==\n");
    printf("clk_sys medido: %" PRIu32 " kHz  (1 ciclo = %" PRIu32 " ps)\n", khz, (uint32_t)(1000000000u / khz));
    printf("N = %u muestras, %u repeticiones, sobrecoste de medida = %" PRIu32 " ciclos\n",
           (unsigned)N, (unsigned)REPS, sobrecoste);
    fila("A float",           &a, khz);
    fila("B entera (actual)", &b, khz);
    fila("C optimizada",      &c, khz);
    cociente_x100("A/B (float -> entera)", a.minimo, b.minimo);
    cociente_x100("B/C (entera -> optimizada)", b.minimo, c.minimo);
    cociente_x100("A/C (total)", a.minimo, c.minimo);

    /* Diferencias de resultado */
    uint32_t dist_cb = 0, dist_ab = 0;
    int dh = 0, ds = 0, dv = 0;
    for (uint32_t i = 0; i < N; i++) {
        if (sal_c[i].h != sal_b[i].h || sal_c[i].s != sal_b[i].s || sal_c[i].v != sal_b[i].v) {
            dist_cb++;
        }
        if (sal_a[i].h != sal_b[i].h || sal_a[i].s != sal_b[i].s || sal_a[i].v != sal_b[i].v) {
            dist_ab++;
        }
        int d = abs_i(sal_a[i].h - sal_b[i].h);
        d = d > 180 ? 360 - d : d;
        dh = d > dh ? d : dh;
        ds = abs_i(sal_a[i].s - sal_b[i].s) > ds ? abs_i(sal_a[i].s - sal_b[i].s) : ds;
        dv = abs_i(sal_a[i].v - sal_b[i].v) > dv ? abs_i(sal_a[i].v - sal_b[i].v) : dv;
    }
    printf("Equivalencia: C difiere de B en %" PRIu32 " de %u muestras; A difiere de B en %" PRIu32
           " (dif. max h=%d s=%d v=%d)\n", dist_cb, (unsigned)N, dist_ab, dh, ds, dv);

    /* Una sola muestra, para el CPI */
    printf("Una muestra (%u,%u,%u): ciclos A=%" PRIu32 " B=%" PRIu32 " C=%" PRIu32 "\n",
           (unsigned)UNA.r, (unsigned)UNA.g, (unsigned)UNA.b,
           ciclos_una(bench_una_a, sobrecoste), ciclos_una(bench_una_b, sobrecoste),
           ciclos_una(bench_una_c, sobrecoste));
    printf("  Contar las instrucciones de bench_una_a/b/c con tools/contar_instrucciones.gdb\n");
    printf("  CPI = ciclos / instrucciones (misma muestra)\n");
}

void prueba_ciclos(void)
{
    ciclos_iniciar();
    color_rapido_iniciar();
    generar_muestras();
    if (!color_rapido_preparar(&cal, &cal_rapida)) {
        printf("calibracion invalida\n");
        for (;;) { }
    }
    for (;;) {
        informe();
        tiempo_esperar_ms(5000);
    }
}
