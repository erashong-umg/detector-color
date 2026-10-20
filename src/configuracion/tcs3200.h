/*
 * tcs3200.h — Parámetros de medición del TCS3200.
 *
 * Escala de frecuencia (S0/S1): HIPÓTESIS, cableado fijo pendiente de confirmar.
 *   S0 S1   escala   fondo de escala típico
 *   L  L    apagado
 *   L  H    2 %      ~12 kHz
 *   H  L    20 %     ~120 kHz   ← propuesta (S0 = 3V3, S1 = GND)
 *   H  H    100 %    ~600 kHz
 * El contador PWM es de 16 bits: en una ventana de 20 ms caben hasta
 * 65535 flancos → 3,27 MHz, de sobra para cualquier escala.
 */
#ifndef CONFIG_TCS3200_H
#define CONFIG_TCS3200_H

/* Ventana de conteo por canal. Más larga = más resolución, más lenta.
   Con 20 % y blanco (~100 kHz) dan ~2000 cuentas → 0,05 % de resolución. */
#define TCS_VENTANA_US          20000u

/* Por debajo de esta frecuencia en el canal claro (sin filtro) se da por hecho que el sensor
   está desconectado o sin alimentación y la lectura no es válida. El mínimo medido en el banco,
   con el negro delante y los LED encendidos, es ≈ 3000 Hz. */
#define TCS_FRECUENCIA_MIN_HZ   200u

/* Espera tras cambiar el filtro (S2/S3) antes de empezar a contar,
   para descartar el periodo en curso del filtro anterior.
   HIPÓTESIS conservadora: ajustar midiendo en el banco. */
#define TCS_ESPERA_FILTRO_US     2000u

#endif /* CONFIG_TCS3200_H */
