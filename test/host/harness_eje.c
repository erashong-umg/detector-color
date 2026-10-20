/* Simulación del eje con el código REAL de eje.c / carro.c / posicion.c / limites.c. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "aplicacion/carro.h"
#include "aplicacion/eje.h"
#include "configuracion/eje.h"
#include "configuracion/motor.h"
#include "drivers/finales.h"
#include "hw/tiempo.h"

/* Tiempo de viaje simulado: el mismo que espera el firmware (configuracion/eje.h). */
#define T_VIAJE (EJE_RECORRIDO_ESPERADO_MS / 1000.0)

uint32_t mock_us;
#ifndef RUIDO
#define RUIDO 0.03
#endif
extern int planta_dir, planta_frenado;
extern double planta_duty;
double planta_x_um;
int ini_roto, fin_roto;

#define LARGO 350000.0
static double K;               /* µm/ms por punto de duty sobre la zona muerta */
static double ZONA_MUERTA;     /* % */
static double signo_real;      /* +1 normal, −1 motor conectado al revés */
static double ruido;           /* variación relativa de la velocidad por viaje */
static int    choque;
static int    dir_prev;

static double aleatorio(void) { return (rand() / (double)RAND_MAX) * 2.0 - 1.0; }

static void planta_inicio(double x0, double t_viaje_s, double zona_muerta, double sig, double rui, int ini_r, int fin_r)
{
    unsigned duty40 = MOTOR_DUTY_MIN_PCT + (100u - MOTOR_DUTY_MIN_PCT) * (EJE_VEL_VIAJE - 1) / 99u;
    ZONA_MUERTA = zona_muerta;
    K = LARGO / (t_viaje_s * 1000.0) / ((double)duty40 - zona_muerta);
    planta_x_um = x0; signo_real = sig; ruido = rui; ini_roto = ini_r; fin_roto = fin_r;
    choque = 0; dir_prev = 0;
}

/* Avanza 1 ms: la planta mueve el carrito según el duty y el sentido eléctricos. */
static void paso_ms(void)
{
    eje_actualizar();
    static double k_viaje = 1.0;
    if (planta_dir != 0 && dir_prev == 0) k_viaje = 1.0 + ruido * aleatorio();
    dir_prev = planta_dir;

    double v = K * k_viaje * fmax(0.0, planta_duty - ZONA_MUERTA);
    planta_x_um += v * planta_dir * signo_real * MOTOR_SIGNO_HACIA_FIN;
    if (planta_x_um < -3000.0 || planta_x_um > LARGO + 3000.0) {
        choque = 1;
        planta_x_um = planta_x_um < 0 ? -3000.0 : LARGO + 3000.0;
    }
    mock_us += 1000;
}

static int hasta(int (*cond)(void), int max_ms)
{
    int n = 0;
    while (!cond() && n < max_ms) { paso_ms(); n++; }
    return n;
}
static int listo_o_error(void) { return eje_estado() == EJE_LISTO || eje_estado() == EJE_ERROR; }

static const char *escenario(const char *nombre, double x0, double t_viaje, double zm, double sig, double rui, int ir, int fr)
{
    planta_inicio(x0, t_viaje, zm, sig, rui, ir, fr);
    carro_iniciar(); eje_iniciar();
    carro_armar(true);
    eje_referenciar();
    int ms = hasta(listo_o_error, 120000);
    printf("[%-44s] -> %-8s", nombre, eje_estado_texto(eje_estado()));
    if (eje_estado() == EJE_ERROR) printf(" (%s)", eje_error_texto(eje_error()));
    printf(" | x real %6.1f mm | calibrado=%d dur=%4u ms | choque=%d | %d ms\n",
           planta_x_um / 1000.0, eje_calibrado(), (unsigned)eje_recorrido_ms(), choque, ms);
    return nombre;
}

int main(void)
{
    srand(7);
    printf("=== Homing + calibración (código real en C) ===\n");
    escenario("arranque en el medio",              175000, T_VIAJE, 15, +1, 0.0, 0, 0);
    escenario("arranque en el INICIO",             0,      T_VIAJE, 15, +1, 0.0, 0, 0);
    escenario("arranque en el FINAL",              350000, T_VIAJE, 15, +1, 0.0, 0, 0);
    escenario("recorrido el doble del esperado",    100000, T_VIAJE * 2.0, 15, +1, 0.0, 0, 0);
    printf("=== Fallos ===\n");
    escenario("sentido del motor INVERTIDO",       175000, T_VIAJE, 15, -1, 0.0, 0, 0);
    escenario("INVERTIDO pegado al FINAL",         350000, T_VIAJE, 15, -1, 0.0, 0, 0);
    escenario("final FIN estropeado",              175000, T_VIAJE, 15, +1, 0.0, 0, 1);
    escenario("final INICIO estropeado",           175000, T_VIAJE, 15, +1, 0.0, 1, 0);

    printf("=== Destinos tras calibrar, con ruido de ±3 %% en la velocidad de cada viaje ===\n");
    for (int zm = 0; zm <= 25; zm += 12) {
        escenario("calibrado", 175000, T_VIAJE, zm, +1, RUIDO, 0, 0);
        double peor = 0, suma_t = 0; int errores = 0, n_mov = 0;
        static const double objetivos[] = { 50000, 300000, 175000, 87500, 262500, 10000, 340000, 0, 350000, 120000 };
        printf("  zona muerta %2d%%: ", zm);
        for (unsigned i = 0; i < sizeof objetivos / sizeof objetivos[0]; i++) {
            if (!eje_ir_a((int32_t)objetivos[i])) { errores++; continue; }
            suma_t += hasta(listo_o_error, 60000); n_mov++;
            for (int k = 0; k < 400; k++) paso_ms();
            double e = (planta_x_um - objetivos[i]) / 1000.0;
            printf("%+5.1f ", e);
            if (fabs(e) > peor) peor = fabs(e);
            if (eje_estado() == EJE_ERROR) { printf("ERROR(%s) ", eje_error_texto(eje_error())); break; }
        }
        printf("| peor %.1f mm, %.1f s/mov, choque=%d, rechazadas=%d\n",
               peor, n_mov ? suma_t / n_mov / 1000.0 : 0.0, choque, errores);
    }

    printf("=== Paseo de repetibilidad: 8 viajes entre 60 y 290 mm (ruido ±3 %%) ===\n");
    escenario("calibrado", 175000, T_VIAJE, 15, +1, RUIDO, 0, 0);
    double amin = 1e9, amax = -1e9, bmin = 1e9, bmax = -1e9;
    for (int i = 0; i < 8; i++) {
        eje_ir_a(60000);  hasta(listo_o_error, 60000); for (int k = 0; k < 400; k++) paso_ms();
        if (planta_x_um < amin) amin = planta_x_um;
        if (planta_x_um > amax) amax = planta_x_um;
        eje_ir_a(290000); hasta(listo_o_error, 60000); for (int k = 0; k < 400; k++) paso_ms();
        if (planta_x_um < bmin) bmin = planta_x_um;
        if (planta_x_um > bmax) bmax = planta_x_um;
    }
    printf("  en 60 mm: %.0f..%.0f mm (dispersión %.1f mm) | en 290 mm: %.0f..%.0f mm (dispersión %.1f mm)\n",
           amin / 1000, amax / 1000, (amax - amin) / 1000, bmin / 1000, bmax / 1000, (bmax - bmin) / 1000);

    printf("=== Seguridad: eje_quieto() y movimiento manual ===\n");
    planta_inicio(175000, T_VIAJE, 15, +1, 0.0, 0, 0); carro_iniciar(); eje_iniciar(); carro_armar(true);
    printf("  parado sin orden: quieto=%d\n", eje_quieto());
    eje_manual(30); for (int k = 0; k < 300; k++) paso_ms();
    printf("  con jog manual:   quieto=%d (debe ser 0), x=%.1f mm\n", eje_quieto(), planta_x_um / 1000);
    eje_manual(0);  for (int k = 0; k < 700; k++) paso_ms();
    printf("  tras soltar:      quieto=%d (debe ser 1)\n", eje_quieto());
    eje_manual(40); int n = 0;
    while (n++ < 40000 && eje_estado() != EJE_ERROR && !final_fin_activo()) paso_ms();
    for (int k = 0; k < 300; k++) paso_ms();
    printf("  jog hasta el final: x=%.1f mm, choque=%d, estado=%s, velocidad real=%d\n",
           planta_x_um / 1000, choque, eje_estado_texto(eje_estado()), carro_velocidad_real());
    return 0;
}
