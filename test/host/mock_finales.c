/* Finales de carrera simulados: leen la posición de la planta y aplican el mismo
   antirrebote por tiempo (FINAL_ANTIREBOTE_MS) que drivers/finales.c. */
#include "drivers/finales.h"
#include "configuracion/motor.h"
#include "hw/tiempo.h"

extern double planta_x_um;
extern int ini_roto, fin_roto;
int forzar_ini, forzar_fin;     /* fuerzan el final como activo (fallo de cableado) */
#define LARGO_UM 350000.0

typedef struct { bool estable, candidato; uint32_t t; } f_t;
static f_t ini, fin;

static bool crudo_ini(void) { return forzar_ini || (!ini_roto && planta_x_um <= 500.0); }
static bool crudo_fin(void) { return forzar_fin || (!fin_roto && planta_x_um >= LARGO_UM - 500.0); }

static void paso(f_t *f, bool lectura, uint32_t ahora)
{
    if (lectura != f->candidato) { f->candidato = lectura; f->t = ahora; }
    else if (lectura != f->estable && (uint32_t)(ahora - f->t) >= FINAL_ANTIREBOTE_MS * 1000u)
        f->estable = lectura;
}

void finales_iniciar(void)
{
    ini.estable = ini.candidato = crudo_ini();
    fin.estable = fin.candidato = crudo_fin();
    ini.t = fin.t = tiempo_us();
}

void finales_actualizar(void)
{
    uint32_t ahora = tiempo_us();
    paso(&ini, crudo_ini(), ahora);
    paso(&fin, crudo_fin(), ahora);
}

bool final_inicio_activo(void) { return ini.estable; }
bool final_fin_activo(void)    { return fin.estable; }
bool final_inicio_crudo(void)  { return crudo_ini(); }
bool final_fin_crudo(void)     { return crudo_fin(); }
