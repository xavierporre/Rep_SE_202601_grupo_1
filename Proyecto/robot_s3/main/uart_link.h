/*
 * uart_link — recepcion de datos de la ESP32-CAM por UART1
 *
 * Pines S3: TX=GPIO17 (reservado hacia la CAM), RX=GPIO18 (datos de la CAM).
 * Protocolo: lineas ASCII "D,<action>,<d0>,<d1>,<d2>,<ident>,<conf>\n"
 * a 115200 8N1, ~10 Hz. Lineas malformadas se descartan en silencio.
 */

#ifndef ROBOT_S3_UART_LINK_H_
#define ROBOT_S3_UART_LINK_H_

#include "robot_state.h"

// Instala el driver UART1 y lanza la tarea de recepcion/parseo
void uart_link_init(void);

// Copia el ultimo dato valido y su antiguedad en ms.
// Devuelve false si aun no se recibio ningun frame valido.
bool uart_link_get(cam_data_t *out, uint32_t *age_ms);

#endif  // ROBOT_S3_UART_LINK_H_
