/*
 * protocolo.h — Protocolo de texto por el puerto serie (USB CDC, 115200).
 *
 * Una línea por mensaje, terminada en '\n' (se acepta también "\r\n"). Sin SDK ni
 * hardware: la entrada llega byte a byte (protocolo_byte) y la salida sale por una
 * función que da quien lo usa, así que se prueba en el PC con el código real.
 *
 * ---- Comandos (sin distinguir mayúsculas) -> respuesta "OK" o "ERR,<motivo>" ----
 *   AYUDA                 lista de comandos
 *   ESTADO                una línea con el estado completo del sistema
 *   LECTURA               la última lectura (color, frecuencias, temperatura, destino)
 *   HIST                  el historial guardado, una línea por lectura
 *   MEDIR                 medir (espera la muestra a la distancia correcta)
 *   CAL NEGRO | CAL BLANCO   calibrar el color
 *   FORZAR                no esperar al IR de proximidad (muestras oscuras)
 *   ARMAR | DESARMAR      puente del motor
 *   REF                   referenciar el eje (busca el inicio y mide el recorrido)
 *   IR <mm>               llevar el carrito a esa posición (admite decimales: IR 160.5)
 *   PARAR                 cancela lo que esté en curso
 *   GUARDAR               guarda la última lectura en el historial
 *   REINICIAR             reinicia un error
 *
 * ---- Mensajes que envía el sistema por su cuenta ----
 *   HOLA,detector-color,protocolo=1        al arrancar
 *   (AUTOREFERENCIANDO lleva desde=INICIO|FIN|MEDIO; AUTOREF_REINTENTO lleva n= y max=)
 *   EVT,<nombre>[,datos]                    cada evento: LISTO, AUTOREFERENCIANDO,
 *       REFERENCIADO, ANALIZANDO, MEDIDA,..., CAL_NEGRO, CAL_BLANCO, LLEGADA,
 *       GUIA,<COLOCAR|ACERCAR|ALEJAR|NINGUNA>, TIEMPO_AGOTADO, CANCELADO,
 *       LECTURA_INVALIDA, ERROR,<texto>, ERROR_REINICIADO
 *
 * Temperaturas: CALIDO, FRIO o NEUTRO (siempre en ASCII).
 */
#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include "aplicacion/sistema.h"

/* Recibe una línea sin el salto final; quien la recibe añade el "\n". */
typedef void (*protocolo_salida_t)(const char *linea);

void protocolo_iniciar(protocolo_salida_t salida);

/* Alimenta un byte recibido; al llegar '\n' (o '\r') procesa la línea. */
void protocolo_byte(int c);

/* Procesa una línea completa (sin salto). */
void protocolo_procesar_linea(const char *linea);

/* Informa de un evento del sistema. */
void protocolo_evento(sistema_evento_t e);

#endif /* PROTOCOLO_H */
