#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Resultado de detección compartido con detection_responder ──────────── */
typedef struct {
    char    edge[8];       /* "none" | "left" | "right" | "center" (legacy) */
    char    action[16];    /* "forward" | "turn_left" | "turn_right" | "stop" */
    int8_t  dist[3];       /* 0=sin borde, 1=lejos, 2=cerca  [izq, centro, der] */
    uint8_t grid[2][3];    /* promedio uint8 por celda [top/bot][L/C/R] */
    bool    identifier;
    int     confidence;
} detection_result_t;

extern detection_result_t g_detection_result;
extern SemaphoreHandle_t  g_detection_mutex;

/* Inicia el servidor HTTP en el puerto 81 (stream + detection) */
void stream_server_start(void);

#ifdef __cplusplus
}
#endif
