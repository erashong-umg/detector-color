/*
 * actuadores.h — Parámetros del buzzer pasivo y del LED RGB (PWM).
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

/* ---- LED RGB de ánodo común (activo en bajo → canales PWM invertidos) ----
   Divisor 16 y TOP = 255 → 125 MHz / 16 / 256 ≈ 30,5 kHz: sin parpadeo
   visible ni en cámara. */
#define LED_DIV_ENTERO      16u
#define LED_TOP             255u

/* Escala por canal (0..255) para igualar el brillo: con 150 Ω en el rojo y
   47 Ω en verde/azul, el verde y el azul pueden dominar. Bajar el canal que
   "gane" hasta que el blanco se vea blanco. */
#define LED_ESCALA_R        255u
#define LED_ESCALA_G        255u
#define LED_ESCALA_B        255u

#endif /* CONFIG_ACTUADORES_H */
