/*
 * control — maquina de estados de los tres comportamientos
 *   FIND (buscar y destruir) / PATROL (patrullar) / RETREAT (retirada)
 * mas MANUAL (los handlers HTTP mueven los motores directamente).
 */

#ifndef ROBOT_S3_CONTROL_H_
#define ROBOT_S3_CONTROL_H_

#include <stddef.h>
#include "robot_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// Lanza la tarea del lazo de control (20 Hz)
void control_start(void);

// Cambia de modo: detiene motores y reinicia el subestado del modo nuevo
void control_set_mode(robot_mode_t m);

robot_mode_t control_get_mode(void);

// Escribe el estado actual como JSON en buf; devuelve los bytes escritos
int control_status_json(char *buf, size_t n);

#ifdef __cplusplus
}
#endif

#endif  // ROBOT_S3_CONTROL_H_
