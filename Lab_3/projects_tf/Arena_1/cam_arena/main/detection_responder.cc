#include "detection_responder.h"
#include "stream_server.h"
#include "esp_log.h"
#include <string.h>

// ── Grilla 3×2: bandas de filas ──────────────────────────────────────────
#define GRID_ROW_TOP_START    0
#define GRID_ROW_TOP_END     47
#define GRID_ROW_BOT_START   48
#define GRID_ROW_BOT_END     95

// Umbral relativo: diferencia mínima entre el promedio global y la celda
#define EDGE_DIFF_THR        10
// Umbral absoluto: luminancia máxima uint8 para considerar la celda como borde
// Rango 0–255; ajustar según iluminación del entorno (valor por defecto: 130)
#define EDGE_ABS_THR        130
// Histeresis: frames consecutivos para confirmar un cambio de acción
#define ACTION_CONFIRM_N      3

static const char *TAG = "cam_arena";

static int8_t        s_last_img[96 * 96];
static volatile bool s_img_ready = false;

void SetLastImage(const int8_t *img) {
    memcpy(s_last_img, img, 96 * 96);
    s_img_ready = true;
}

// ── Análisis de grilla 3×2 con umbral combinado e histeresis ─────────────
static void analyze_grid(uint8_t grid[2][3], int8_t dist[3], char *action_out) {
    if (!s_img_ready) {
        for (int r = 0; r < 2; r++)
            for (int c = 0; c < 3; c++) grid[r][c] = 0;
        for (int c = 0; c < 3; c++) dist[c] = 0;
        strncpy(action_out, "forward", 15);
        return;
    }

    const int WIDTH  = 96;
    const int COL_W  = 32;   // 96 / 3 columnas
    const int row_start[2] = { GRID_ROW_TOP_START, GRID_ROW_BOT_START };
    const int row_end[2]   = { GRID_ROW_TOP_END,   GRID_ROW_BOT_END   };

    // 1. Promedio uint8 por celda (int8 XOR 0x80 → uint8: oscuro≈0, brillante≈255)
    for (int r = 0; r < 2; r++) {
        for (int c = 0; c < 3; c++) {
            long sum = 0;
            int  cnt = 0;
            int  x0  = c * COL_W;
            int  x1  = x0 + COL_W;
            for (int y = row_start[r]; y <= row_end[r]; y++) {
                for (int x = x0; x < x1; x++) {
                    sum += (uint8_t)s_last_img[y * WIDTH + x];
                    cnt++;
                }
            }
            grid[r][c] = (uint8_t)(sum / cnt);
        }
    }

    // 2. Promedio global de las 6 celdas
    int global_sum = 0;
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 3; c++)
            global_sum += grid[r][c];
    int global_mean = global_sum / 6;

    // 3. Detección de borde: umbral combinado (relativo Y absoluto)
    //    Evita falsos positivos cuando toda la imagen es oscura
    bool edge[2][3];
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 3; c++)
            edge[r][c] = (grid[r][c] < EDGE_ABS_THR) &&
                         ((global_mean - (int)grid[r][c]) >= EDGE_DIFF_THR);

    // 4. Distancia por columna: 2=cerca (bot), 1=lejos (solo top), 0=sin borde
    for (int c = 0; c < 3; c++) {
        if      (edge[1][c]) dist[c] = 2;
        else if (edge[0][c]) dist[c] = 1;
        else                 dist[c] = 0;
    }

    // 5. Lógica de acción (prioridad: bordes cercanos primero)
    const char *new_action;
    if      (dist[1] == 2)                 new_action = "stop";
    else if (dist[0] == 2 && dist[2] == 2) new_action = "stop";
    else if (dist[0] == 2)                 new_action = "turn_right";
    else if (dist[2] == 2)                 new_action = "turn_left";
    else                                   new_action = "forward";

    // 6. Histeresis: confirmar ACTION_CONFIRM_N frames antes de cambiar acción
    static char    s_confirmed[16] = "forward";
    static char    s_pending[16]   = "forward";
    static uint8_t s_count         = 0;

    if (strcmp(new_action, s_confirmed) != 0) {
        if (strcmp(new_action, s_pending) == 0) {
            if (++s_count >= ACTION_CONFIRM_N) {
                strncpy(s_confirmed, new_action, sizeof(s_confirmed) - 1);
                s_count = 0;
            }
        } else {
            strncpy(s_pending, new_action, sizeof(s_pending) - 1);
            s_count = 1;
        }
    } else {
        s_count = 0;
    }

    strncpy(action_out, s_confirmed, 15);
    action_out[15] = '\0';
}

void RespondToDetection(float, float) {
    uint8_t grid[2][3];
    int8_t  dist[3];
    char    action[16];

    analyze_grid(grid, dist, action);

    // Campo legacy 'edge': columna con menor promedio en la fila inferior
    const char *edge;
    if (dist[0] == 0 && dist[1] == 0 && dist[2] == 0) {
        edge = "none";
    } else {
        uint8_t min_val = grid[1][0];
        edge = "left";
        if (grid[1][1] < min_val) { min_val = grid[1][1]; edge = "center"; }
        if (grid[1][2] < min_val) {                        edge = "right";  }
    }

    // Log solo cuando cambia la acción (reduce carga de UART)
    static char s_last_action_log[16] = "";
    if (strcmp(action, s_last_action_log) != 0) {
        ESP_LOGI(TAG, "[BORDE] accion=%s  borde=%s  dist=[%d,%d,%d]",
                 action, edge, (int)dist[0], (int)dist[1], (int)dist[2]);
        strncpy(s_last_action_log, action, sizeof(s_last_action_log) - 1);
    }

    if (xSemaphoreTake(g_detection_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        strncpy(g_detection_result.edge,   edge,   sizeof(g_detection_result.edge)   - 1);
        strncpy(g_detection_result.action, action, sizeof(g_detection_result.action) - 1);
        for (int c = 0; c < 3; c++)
            g_detection_result.dist[c] = dist[c];
        for (int r = 0; r < 2; r++)
            for (int c = 0; c < 3; c++)
                g_detection_result.grid[r][c] = grid[r][c];
        g_detection_result.identifier = false;
        g_detection_result.confidence = 0;
        xSemaphoreGive(g_detection_mutex);
    }
}

void create_gui() {}
