/*
 * stream_server.cc — Servidor de detección en puerto 81
 *
 * Endpoint:
 *   GET /detection → JSON {"edge":"none","identifier":false,"confidence":0}
 *
 * El stream MJPEG fue eliminado para liberar recursos.
 * La cámara permanece siempre en GRAYSCALE 96×96.
 */

#include "stream_server.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "stream_server";

/* ── Globales compartidos ────────────────────────────────────────────────── */
SemaphoreHandle_t g_detection_mutex = NULL;
detection_result_t g_detection_result = {"none", "forward", {0,0,0}, {{0,0,0},{0,0,0}}, false, 0};

/* ── Handler /detection ──────────────────────────────────────────────────── */
static esp_err_t detection_handler(httpd_req_t *req)
{
    char json[200];
    detection_result_t r;

    if (xSemaphoreTake(g_detection_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        r = g_detection_result;
        xSemaphoreGive(g_detection_mutex);
    } else {
        r = g_detection_result;
    }

    snprintf(json, sizeof(json),
             "{\"edge\":\"%s\",\"action\":\"%s\","
             "\"dist\":[%d,%d,%d],"
             "\"grid\":[[%d,%d,%d],[%d,%d,%d]],"
             "\"identifier\":%s,\"confidence\":%d}",
             r.edge, r.action,
             (int)r.dist[0], (int)r.dist[1], (int)r.dist[2],
             (int)r.grid[0][0], (int)r.grid[0][1], (int)r.grid[0][2],
             (int)r.grid[1][0], (int)r.grid[1][1], (int)r.grid[1][2],
             r.identifier ? "true" : "false",
             r.confidence);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

/* ── Inicio del servidor ─────────────────────────────────────────────────── */
void stream_server_start(void)
{
    g_detection_mutex = xSemaphoreCreateMutex();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port      = 81;
    config.ctrl_port        = 32769;
    config.max_uri_handlers = 2;
    config.stack_size       = 4096;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Error al iniciar servidor puerto 81");
        return;
    }

    httpd_uri_t uri_detection = { .uri="/detection", .method=HTTP_GET, .handler=detection_handler, .user_ctx=NULL };
    httpd_register_uri_handler(server, &uri_detection);

    ESP_LOGI(TAG, "Deteccion: http://[ip]:81/detection");
}
