#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Resultado de detección compartido con detection_responder ──────────── */
typedef struct {
    char edge[8];      /* "none" | "left" | "right" | "center" */
    bool identifier;
    int  confidence;
} detection_result_t;

extern detection_result_t g_detection_result;
extern SemaphoreHandle_t  g_detection_mutex;

/* Inicia el servidor HTTP en el puerto 81 (stream + detection) */
void stream_server_start(void);

#ifdef __cplusplus
}
#endif
