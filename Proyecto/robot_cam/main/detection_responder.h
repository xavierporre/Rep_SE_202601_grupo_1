/*
 * detection_responder — combina:
 *  - deteccion de borde por grilla 3x2 (de Arena_1/cam_arena)
 *  - clasificacion del identificador "embutido" (de cam_identificador)
 * y publica el resultado por UART hacia el ESP32-S3 (uart_comm).
 */

#ifndef ROBOT_CAM_DETECTION_RESPONDER_H_
#define ROBOT_CAM_DETECTION_RESPONDER_H_

#include "tensorflow/lite/c/common.h"

// Procesa el resultado de la ultima inferencia (scores en [0,1]) junto con el
// analisis de borde del ultimo frame, y publica todo por UART.
void RespondToDetection(float person_score, float no_person_score);

// Almacena el ultimo frame int8 96x96 para el analisis de borde
void SetLastImage(const int8_t *img);

// Stub requerido por main_functions (sin display)
void create_gui();

#endif  // ROBOT_CAM_DETECTION_RESPONDER_H_
