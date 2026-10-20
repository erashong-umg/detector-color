/*
 * sistema.h — Parámetros de la máquina de estados.
 */
#ifndef CONFIG_SISTEMA_H
#define CONFIG_SISTEMA_H

/* Tras pedir medir o calibrar, tiempo máximo esperando a que el objeto esté a
   la distancia correcta. Pasado, se cancela. */
#define SISTEMA_ESPERA_OBJETO_MS    8000u

/* El objeto debe estar a la distancia correcta de forma continua este tiempo
   antes de medir (evita medir mientras se está colocando). */
#define SISTEMA_PROX_ESTABLE_MS     300u

/* Al encender, el sistema arma el motor y referencia el eje solo: busca el final
   de INICIO, mide el recorrido y vuelve al inicio. El carrito tiene que estar libre
   para moverse. Poner a 0 para armar y referenciar a mano (menú o B1/B2). */
#ifndef SISTEMA_AUTOREFERENCIAR
#define SISTEMA_AUTOREFERENCIAR     1
#endif

/* Espera tras el arranque antes de empezar a moverse (deja ver la pantalla y que se
   asienten los finales de carrera). */
#define SISTEMA_AUTOREF_RETARDO_MS  2000u

/* Al encender, la pantalla abre un asistente que guía paso a paso: referenciar el eje,
   calibrar el negro y calibrar el blanco. La calibración del color vive en RAM, así que
   hay que repetirla tras cada corte de corriente. B4 largo omite el asistente. */
#ifndef ASISTENTE_AL_INICIAR
#define ASISTENTE_AL_INICIAR        1
#endif

/* Si la referencia automática falla por algo recuperable (tiempo máximo agotado o
   calibración demasiado corta), se reintenta sola hasta SISTEMA_AUTOREF_INTENTOS veces más,
   esperando SISTEMA_AUTOREF_REINTENTO_MS entre intentos. No se reintenta si los finales son
   incoherentes ni si el sentido del motor está invertido: eso necesita a una persona. */
#define SISTEMA_AUTOREF_INTENTOS    2
#define SISTEMA_AUTOREF_REINTENTO_MS 1000u

/* Recuperación automática tras desconectar los finales de carrera. Un cable suelto se lee
   como "activo", y con los dos activos a la vez el sistema se detiene en ERROR. Cuando los
   finales vuelven a ser coherentes durante SISTEMA_RECUPERAR_MS, el error se reinicia solo y
   se vuelve a referenciar el eje (la posición se pierde). Poner a 0 para reiniciar a mano
   (B4 largo o el comando REINICIAR). */
#ifndef SISTEMA_AUTORECUPERAR
#define SISTEMA_AUTORECUPERAR       1
#endif
#define SISTEMA_RECUPERAR_MS        500u

#endif /* CONFIG_SISTEMA_H */
