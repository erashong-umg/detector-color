/*
 * escala.h — Correspondencia color → posición del carrito sobre el eje X.
 *
 * El carrito (poleas y correa sobre un eje lineal X, movido por el TB6612FNG)
 * va a UNA de tres posiciones fijas según la temperatura del color:
 *     cálido (rojo, naranja, amarillo, magenta rojizo) → izquierda
 *     neutro (negro, gris, blanco: sin tono)           → centro
 *     frío   (verde, cian, azul, violeta)              → derecha
 * Los umbrales de tono y de clasificación son estimaciones: ajustarlos con
 * muestras reales (los colores impresos y los medidos por el TCS3200 no
 * coincidirán exactamente).
 */
#ifndef CONFIG_ESCALA_H
#define CONFIG_ESCALA_H

/* Ancho de la zona de trabajo (µm): se reparte en tres tercios. */
#define ESCALA_ANCHO_UM             320000
/* Distancia entre el final de carrera INICIO y el borde izquierdo impreso de
   la escala (µm). HIPÓTESIS: 0 (la escala empieza justo en el sensor); medir
   con regla poniendo el cursor en el inicio. Junto con EJE_LONGITUD_UM
   (recorrido real entre los dos sensores) debe cumplirse:
   desplazamiento + ancho ≤ recorrido. */
#define ESCALA_DESPLAZAMIENTO_UM    0
/* Cálido / frío por tono (0..359°). Cálido: de ESCALA_TONO_CALIDO_DESDE hasta
   360° y de 0° hasta ESCALA_TONO_FRIO_DESDE; frío: el resto. */
#define ESCALA_TONO_FRIO_DESDE      75      /* amarillo-verde: aquí empieza el frío  */
#define ESCALA_TONO_CALIDO_DESDE    315     /* magenta-rojo: aquí empieza el cálido  */

/* Posiciones de parada (µm desde el final de carrera INICIO): el centro de
   cada tercio de la zona de trabajo. */
#define ESCALA_POS_CALIDO_UM        (ESCALA_DESPLAZAMIENTO_UM + ESCALA_ANCHO_UM / 6)
#define ESCALA_POS_NEUTRO_UM        (ESCALA_DESPLAZAMIENTO_UM + ESCALA_ANCHO_UM / 2)
#define ESCALA_POS_FRIO_UM          (ESCALA_DESPLAZAMIENTO_UM + ESCALA_ANCHO_UM * 5 / 6)

/* Clasificación de acromáticos (S y V en 0..255). */
#define ESCALA_VAL_NEGRO            40      /* V por debajo: negro, el tono no es fiable */
#define ESCALA_SAT_MIN              45      /* S por debajo: sin color (blanco o gris)   */
#define ESCALA_VAL_BLANCO           180     /* sin color y V ≥ esto: blanco, si no gris  */

/* No se vuelve a mover si el cursor ya está a menos de esta distancia. */
#define ESCALA_ZONA_MUERTA_UM       3000

/* Selector de color del modo manual (dominio/selector.c). */
#define SELECTOR_VALOR              200     /* brillo fijo del color elegido (0..255)               */
#define SELECTOR_PASO_TONO_X1000    12      /* milésimas de grado por punto de palanca y llamada
                                               (cada 10 ms): a tope 1,2° por llamada ≈ 120°/s       */
#define SELECTOR_PASO_SATURACION    32      /* saturación por pulso de arriba / abajo               */

/* Mediciones promediadas por lectura (cada una tarda ≈ 90 ms). */
#define DETECCION_MUESTRAS          3

#endif /* CONFIG_ESCALA_H */
