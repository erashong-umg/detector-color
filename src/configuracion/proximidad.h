/*
 * proximidad.h — IR de proximidad junto al TCS3200: presencia y distancia de la muestra.
 *
 * TODO lo marcado HIPÓTESIS hay que medirlo en el banco con
 * pruebas/prueba_proximidad.c: la polaridad y los valores dependen del módulo
 * concreto y de la reflectividad de la muestra.
 */
#ifndef CONFIG_PROXIMIDAD_H
#define CONFIG_PROXIMIDAD_H

/* 0 = el sensor no está conectado: medir y calibrar no esperan al objeto. */
#define PROX_HABILITADO              1

/* Salida digital OD. HIPÓTESIS: activa en BAJO (nivel bajo = hay objeto), lo
   habitual en los módulos con comparador LM393. El pull interno apunta hacia
   "sin objeto": con el cable suelto se lee como que no hay nada. */
#define PROX_DIGITAL_ACTIVO_BAJO     1

/* Salida analógica OA. HIPÓTESIS: la tensión BAJA al acercarse el objeto
   (módulos tipo TCRT5000). Con 1 se invierte la lectura para trabajar siempre
   con "nivel": mayor = más cerca. */
#define PROX_ANALOGICO_INVERTIDO     1

/* Umbrales sobre el nivel (0..4095, mayor = más cerca). HIPÓTESIS: medir con
   el objeto a distintas distancias y ajustar.
     nivel < PRESENCIA              sin objeto
     PRESENCIA ≤ nivel < OK         hay objeto pero lejos: "acerca el objeto"
     OK ≤ nivel ≤ CERCA             distancia correcta para medir
     nivel > CERCA                  demasiado cerca                            */
#define PROX_NIVEL_PRESENCIA         400
#define PROX_NIVEL_OK                1500
#define PROX_NIVEL_CERCA             3990   /* OA se satura (~3830..4040) con cualquier objeto: "cerca" solo con ADC <= 5, es decir, nunca en la práctica */

/* Histéresis alrededor de cada umbral, para que no oscile en el límite. */
#define PROX_HISTERESIS              100

/* Un estado nuevo se acepta tras este número de lecturas seguidas iguales. */
#define PROX_MUESTRAS_ESTABLES       6
#define PROX_PERIODO_MS              5
#define PROX_MUESTRAS_ADC            4

#endif /* CONFIG_PROXIMIDAD_H */
