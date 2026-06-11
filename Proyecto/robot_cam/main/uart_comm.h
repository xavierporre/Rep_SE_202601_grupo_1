/*
 * uart_comm — enlace serie ESP32-CAM → ESP32-S3
 *
 * UART1 por matriz de GPIO: TX=GPIO14, RX=GPIO13 (pines SD libres sin tarjeta;
 * NUNCA usar GPIO12: strapping MTDI, un nivel alto en el arranque fija el
 * voltaje de flash en 1.8 V y la placa no inicia).
 *
 * Protocolo (linea ASCII, 115200 8N1, 10 Hz):
 *   D,<action>,<d0>,<d1>,<d2>,<ident>,<conf>\n
 *   action: forward|turn_left|turn_right|stop   d0..d2: 0|1|2
 *   ident: 0|1 (score >= 0.75)                  conf: 0..100 (%)
 */

#ifndef ROBOT_CAM_UART_COMM_H_
#define ROBOT_CAM_UART_COMM_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char   action[16];  // accion sugerida por el analisis de borde
    int8_t dist[3];     // distancia por columna [izq, centro, der]
    bool   ident;       // identificador (embutido) detectado
    int    conf;        // confianza 0..100
} cam_result_t;

// Inicializa UART1 y lanza la tarea de transmision a 10 Hz
void uart_comm_init(void);

// Publica el ultimo resultado (lo consume la tarea TX)
void uart_comm_publish(const cam_result_t *res);

#ifdef __cplusplus
}
#endif

#endif  // ROBOT_CAM_UART_COMM_H_
