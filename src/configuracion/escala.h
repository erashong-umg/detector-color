/*
 * escala.h — Correspondencia color → posición sobre la escala impresa.
 *
 * HIPÓTESIS: la escala impresa es un gradiente lineal de tono (hue) de 0° a
 * 360° que empieza y termina en rojo, sobre 350 mm. Los umbrales de
 * clasificación son estimaciones: ajustarlos con muestras reales (los
 * colores impresos y los medidos por el TCS3200 no coincidirán exactamente).
 */
#ifndef CONFIG_ESCALA_H
#define CONFIG_ESCALA_H

/* Ancho de la zona impresa (µm): lo que mide la escala con una regla. */
#define ESCALA_ANCHO_UM             320000
/* Distancia entre el final de carrera INICIO y el borde izquierdo impreso de
   la escala (µm). HIPÓTESIS: 0 (la escala empieza justo en el sensor); medir
   con regla poniendo el cursor en el inicio. Junto con EJE_LONGITUD_UM
   (recorrido real entre los dos sensores) debe cumplirse:
   desplazamiento + ancho ≤ recorrido. */
#define ESCALA_DESPLAZAMIENTO_UM    0
/* Tono impreso en el extremo izquierdo de la escala. 0 = empieza en rojo. */
#define ESCALA_TONO_INICIAL_GRADOS  0

/* Rojo: el tono 0° y el 360° son el mismo color y ambos extremos de la
   escala están impresos en rojo. Dentro de ± esta banda el cursor va al
   extremo más cercano a donde ya está, en vez de saltar de un extremo al
   otro por el ruido de la medida (358° ↔ 2°). */
#define ESCALA_BANDA_ROJO_GRADOS    10

/* Clasificación de acromáticos (S y V en 0..255). */
#define ESCALA_VAL_NEGRO            40      /* V por debajo: negro, el tono no es fiable */
#define ESCALA_SAT_MIN              45      /* S por debajo: sin color (blanco o gris)   */
#define ESCALA_VAL_BLANCO           180     /* sin color y V ≥ esto: blanco, si no gris  */

/* Política para colores sin tono (negro, gris, blanco):
     0 = no mover el cursor (solo avisar)
     1 = llevarlo al inicio (posición de reposo)                            */
#define ESCALA_ACROMATICO_AL_INICIO 0

/* No se vuelve a mover si el cursor ya está a menos de esta distancia. */
#define ESCALA_ZONA_MUERTA_UM       3000

/* Mediciones promediadas por lectura (cada una tarda ≈ 90 ms). */
#define DETECCION_MUESTRAS          3

#endif /* CONFIG_ESCALA_H */
