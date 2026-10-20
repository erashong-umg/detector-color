/*
 * app.h — Aplicación completa: une la máquina de estados (sistema) con la
 * interfaz y los dispositivos de entrada y salida (pantalla, botones,
 * joystick y buzzer) y con el puerto serie (protocolo.h). No retorna.
 *
 * leer_caracter: devuelve el siguiente byte recibido por el puerto serie o −1 si no hay
 * ninguno (sin esperar). Lo da main.c, que es el único que usa el SDK. NULL = sin entrada.
 */
#ifndef APP_H
#define APP_H

void app_ejecutar(int (*leer_caracter)(void));

#endif /* APP_H */
