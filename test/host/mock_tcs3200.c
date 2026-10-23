/* TCS3200 simulado: devuelve las frecuencias que fija la prueba y gasta el mismo
   tiempo que el real (≈ 4 × (2 ms de espera + 20 ms de ventana)). */
#include "drivers/tcs3200.h"
#include "drivers/tcs3200_async.h"
#include "hw/tiempo.h"

tcs_frecuencias_t mock_frecuencias;
int mock_tcs_desborde;
int mock_tcs_llamadas;

void tcs3200_iniciar(void) { }
void tcs3200_poner_espera(void (*hook)(void)) { (void)hook; }

uint32_t tcs3200_medir_canal(tcs_filtro_t filtro, bool *desborde)
{
    (void)filtro;
    *desborde = mock_tcs_desborde != 0;
    return 0;
}

bool tcs3200_medir(tcs_frecuencias_t *f)
{
    mock_tcs_llamadas++;
    mock_us += 4u * 22000u;     /* la medición bloquea ≈ 88 ms */
    *f = mock_frecuencias;
    return !mock_tcs_desborde;
}

/* Adquisición por ISR simulada: entrega un juego R-G-B-C cada 88 ms de reloj simulado, como la
   real (4 canales × (2 ms de filtro + 20 ms de ventana)). */
int mock_tcs_async_activo;
static uint32_t async_t0;

void tcs3200_async_iniciar(void)
{
    mock_tcs_async_activo = 1;
    async_t0 = tiempo_us();
}

void tcs3200_async_detener(void) { mock_tcs_async_activo = 0; }

bool tcs3200_async_tomar(tcs_frecuencias_t *f, bool *valido)
{
    if (!mock_tcs_async_activo || (uint32_t)(tiempo_us() - async_t0) < 88000u) {
        return false;
    }
    async_t0 += 88000u;
    mock_tcs_llamadas++;
    *f = mock_frecuencias;
    if (valido) {
        *valido = !mock_tcs_desborde;
    }
    return true;
}
