/*
 * eje.h — Parámetros del eje lineal (carrito sobre 350 mm) y del
 * posicionamiento por tiempo.
 *
 * HIPÓTESIS: el modelo "recorrido ∝ ∫ velocidad dt" es aproximado; el motor
 * real tiene zona muerta y no es proporcional al ciclo de trabajo. Se compensa
 * calibrando con la MISMA velocidad que se usa al viajar y resincronizando en
 * cada final de carrera. Ajustar con la prueba de repetibilidad
 * (pruebas/prueba_eje.c) y una regla.
 */
#ifndef CONFIG_EJE_H
#define CONFIG_EJE_H

/* Longitud útil de la escala, en micrómetros (350 mm). */
#define EJE_LONGITUD_UM        320000

/* Velocidades (% de 1..100).
   IMPORTANTE: EJE_VEL_APROX debe ser IGUAL a EJE_VEL_VIAJE. El modelo de
   posición supone recorrido proporcional a la velocidad ordenada, y un motor
   DC real no lo cumple (zona muerta): solo es exacto a la velocidad con la que
   se calibró. Simulado con el código real: con 25 % de aproximación el error
   sistemático llega a 9-19 mm según la zona muerta; con la misma velocidad
   queda en ≤ 1,6 mm. Solo bajarla si el carrito frena mal y se acepta ese error. */
#define EJE_VEL_HOMING         30       /* búsqueda del inicio, despacio  */
#define EJE_VEL_VIAJE          40
#ifndef EJE_VEL_APROX
#define EJE_VEL_APROX          EJE_VEL_VIAJE
#endif

/* Dentro de esta distancia del destino se baja a EJE_VEL_APROX (sin efecto si
   es igual a EJE_VEL_VIAJE). */
#define EJE_DIST_APROX_UM      12000

/* Distancia que el carrito sigue avanzando tras frenar a velocidad de viaje.
   HIPÓTESIS: 0. Medirla en el banco (marcar dónde empieza a frenar y dónde
   queda) y ponerla aquí: se frena esa distancia antes del destino. */
#define EJE_DIST_FRENO_UM      0
/* El destino se da por alcanzado a menos de esta distancia (≈ resolución). */
#define EJE_TOLERANCIA_UM      1500

/* Tiempo que tarda el carrito en recorrer los 350 mm a EJE_VEL_VIAJE.
   HIPÓTESIS (8 s): medirlo con pruebas/prueba_motor.c o con la primera
   calibración (el USB imprime "recorrido de N ms"). De este valor salen los
   tiempos máximos de movimiento, así que sin encoder es la ÚNICA protección
   si un final de carrera no se activa: el motor empujará contra el tope
   hasta que venza. Cuanto más exacto, menos tiempo empuja. */
#define EJE_RECORRIDO_ESPERADO_MS   8000u

/* Homing: a EJE_VEL_HOMING (más lento) el recorrido completo tarda ≈ 1,3 ×. */
#define EJE_TIMEOUT_HOMING_MS       (EJE_RECORRIDO_ESPERADO_MS * 2u)
/* Calibración: 1,4 × el recorrido esperado. */
#define EJE_TIMEOUT_CALIB_MS        (EJE_RECORRIDO_ESPERADO_MS * 7u / 5u)
/* Tras calibrar, cada movimiento admite 1,4 × el recorrido medido + margen. */
#define EJE_TIMEOUT_MARGEN_MS       500u

/* Referencia previa: para ir a un punto intermedio, si el último tramo desde el
   final de carrera más cercano al destino es más corto que el viaje directo
   (en al menos EJE_REF_PREVIA_AHORRO_UM), se va PRIMERO a ese final, donde la
   posición se resincroniza con error cero, y se recorre solo el último tramo.
   Sin encoder, el error crece con la distancia recorrida (≈ la variación de
   velocidad del motor × la distancia): así queda acotado por min(x, L−x).
   Cuesta tiempo extra. Poner a 0 para viajar siempre en directo. */
#ifndef EJE_REFERENCIA_PREVIA
#define EJE_REFERENCIA_PREVIA       1
#endif
#define EJE_REF_PREVIA_AHORRO_UM    20000

/* Una calibración más corta que esto indica sensores mal puestos
   (p. ej. los dos finales tapados o los cables cruzados). */
#define EJE_RECORRIDO_MIN_MS   800u

#endif /* CONFIG_EJE_H */
