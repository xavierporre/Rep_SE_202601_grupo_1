#include "detection_responder.h"
#include "stream_server.h"
#include "esp_log.h"
#include <string.h>

#define EDGE_ROW_START  80
#define EDGE_ROW_END    95
#define EDGE_DIFF_THR   10   /* diferencia mínima entre columnas para declarar borde */

static const char *TAG = "cam_arena";

static int8_t s_last_img[96 * 96];
static volatile bool s_img_ready = false;

void SetLastImage(const int8_t *img) {
    memcpy(s_last_img, img, 96 * 96);
    s_img_ready = true;
}

static const char *analyze_edge(void) {
    if (!s_img_ready) return "none";

    int rows = EDGE_ROW_END - EDGE_ROW_START + 1;
    long sum_l = 0, sum_c = 0, sum_r = 0;

    for (int row = EDGE_ROW_START; row <= EDGE_ROW_END; row++) {
        for (int col = 0;  col < 32; col++) sum_l += s_last_img[row * 96 + col];
        for (int col = 32; col < 64; col++) sum_c += s_last_img[row * 96 + col];
        for (int col = 64; col < 96; col++) sum_r += s_last_img[row * 96 + col];
    }

    int avg_l = (int)(sum_l / (rows * 32));
    int avg_c = (int)(sum_c / (rows * 32));
    int avg_r = (int)(sum_r / (rows * 32));

    int min_val = avg_l;
    if (avg_c < min_val) min_val = avg_c;
    if (avg_r < min_val) min_val = avg_r;

    int max_val = avg_l;
    if (avg_c > max_val) max_val = avg_c;
    if (avg_r > max_val) max_val = avg_r;
    if ((max_val - min_val) < EDGE_DIFF_THR) return "none";

    if      (min_val == avg_l) return "left";
    else if (min_val == avg_r) return "right";
    else                       return "center";
}

void RespondToDetection(float, float) {
    const char *edge = analyze_edge();
    ESP_LOGI(TAG, "[BORDE] %s", edge);

    if (xSemaphoreTake(g_detection_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        strncpy(g_detection_result.edge, edge, sizeof(g_detection_result.edge) - 1);
        g_detection_result.identifier = false;
        g_detection_result.confidence = 0;
        xSemaphoreGive(g_detection_mutex);
    }
}

void create_gui() {}
