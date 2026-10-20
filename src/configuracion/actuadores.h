/*
 * actuadores.h — Parámetros del buzzer pasivo (PWM).
 */
#ifndef CONFIG_ACTUADORES_H
#define CONFIG_ACTUADORES_H

/* El runtime del SDK deja clk_sys a 125 MHz (PLL del sistema). El PWM
   cuenta con ese reloj; si se cambia, hay que cambiar esta constante. */
#define CLK_SYS_HZ          125000000u

/* ---- Buzzer pasivo (slice 7, canal B) ----
   Divisor fijo 16 → contador a 7,8125 MHz. Con TOP de 16 bits la nota más
   grave es 7 812 500 / 65 536 ≈ 119 Hz; a 5 kHz TOP vale ≈ 1562 (0,06 %). */
#define BUZZER_DIV_ENTERO   16u
#define BUZZER_FREC_MIN_HZ  120u
#define BUZZER_FREC_MAX_HZ  15000u
/* Ciclo de trabajo en %: 50 = volumen máximo (onda cuadrada simétrica);
   valores menores suenan más bajo y gastan menos corriente. */
#define BUZZER_VOLUMEN_PCT  40u

#endif /* CONFIG_ACTUADORES_H */
