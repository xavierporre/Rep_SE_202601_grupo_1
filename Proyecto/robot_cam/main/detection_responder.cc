#include "detection_responder.h"
#include "uart_comm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include <string.h>

// DIAGNOSTICO TEMPORAL (Fase 0): mide la Hz real de inferencia. Quitar al terminar.
#define DIAG_INFERENCE_HZ_LOG_EVERY_N  20

// ── Grilla 3×2: bandas de filas ──────────────────────────────────────────
#define GRID_ROW_TOP_START    0
#define GRID_ROW_TOP_END     47
#define GRID_ROW_BOT_START   48
#define GRID_ROW_BOT_END     95

// Umbral relativo: diferencia mínima entre el promedio global y la celda
#define EDGE_DIFF_THR        10
// Umbral absoluto: luminancia máxima uint8 para considerar la celda como borde
// Rango 0–255; ajustar según iluminación del ring (valor por defecto: 130)
#define EDGE_ABS_THR        130
// Histeresis: frames consecutivos para confirmar un cambio de acción
#define ACTION_CONFIRM_N      3

// Umbral del identificador (modelo embutido) — alineado con IDENT_CONF_THR
// del S3 (robot_state.h) para que el LED refleje el mismo criterio que
// efectivamente decide el comportamiento del robot.
#define IDENT_THRESHOLD    0.50f
// Suavizado del score (EMA): reduce falsos negativos por ruido frame a frame.
// Alpha bajo = mas memoria (no perder el identificador por una caida puntual
// del score); ajustar segun la Hz de inferencia real medida en campo (Fase 0).
#define IDENT_EMA_ALPHA    0.18f
// Histeresis: frames consecutivos de score bajo el umbral antes de soltar el
// identificador. La subida no necesita histeresis (reaccionar rapido al
// aparecer el objetivo); solo la perdida se confirma para evitar que un
// frame ruidoso lo haga desaparecer "casi de inmediato".
#define IDENT_LOST_CONFIRM_N  4
// LED flash AI-Thinker: encendido mientras el identificador está detectado
#define LED_FLASH_GPIO     GPIO_NUM_4

static const char *TAG = "robot_cam";

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

    snprintf(action_out, 16, "%s", s_confirmed);
}

void RespondToDetection(float person_score, float no_person_score) {
    // DIAGNOSTICO TEMPORAL (Fase 0): Hz real de inferencia. Quitar al terminar.
    {
        static int64_t s_last_us = 0;
        static int     s_calls   = 0;
        int64_t now_us = esp_timer_get_time();
        if (s_last_us != 0 && (++s_calls % DIAG_INFERENCE_HZ_LOG_EVERY_N) == 0) {
            int64_t delta_us = now_us - s_last_us;
            float   hz = (delta_us > 0)
                ? (DIAG_INFERENCE_HZ_LOG_EVERY_N * 1000000.0f / delta_us)
                : 0.0f;
            ESP_LOGI(TAG, "[DIAG] inferencia ~%.2f Hz (delta %lld us / %d frames)",
                     hz, (long long)delta_us, DIAG_INFERENCE_HZ_LOG_EVERY_N);
            s_last_us = now_us;
        } else if (s_last_us == 0) {
            s_last_us = now_us;
        }
    }

    uint8_t grid[2][3];
    int8_t  dist[3];
    char    action[16];

    analyze_grid(grid, dist, action);

    // EMA del score: evita que un solo frame ruidoso por debajo del umbral
    // tire abajo el identificador (antes se evaluaba el score crudo).
    static float s_score_ema = 0.0f;
    s_score_ema += IDENT_EMA_ALPHA * (person_score - s_score_ema);

    int conf = (int)(s_score_ema * 100 + 0.5f);
    if (conf < 0)   conf = 0;
    if (conf > 100) conf = 100;

    // Histeresis de perdida: el identificador solo se suelta tras
    // IDENT_LOST_CONFIRM_N frames consecutivos bajo el umbral, para no
    // perderlo "casi de inmediato" por un solo frame ruidoso. La subida es
    // inmediata (sin retraso al aparecer el objetivo).
    static bool    s_ident_confirmed = false;
    static uint8_t s_lost_count      = 0;
    bool above = (s_score_ema >= IDENT_THRESHOLD);
    if (above) {
        s_ident_confirmed = true;
        s_lost_count      = 0;
    } else if (s_ident_confirmed) {
        if (++s_lost_count >= IDENT_LOST_CONFIRM_N) {
            s_ident_confirmed = false;
            s_lost_count      = 0;
        }
    }
    bool ident = s_ident_confirmed;

    gpio_set_level(LED_FLASH_GPIO, ident ? 1 : 0);

    // Log solo cuando cambia la acción o el identificador (reduce carga de UART0)
    static char s_last_action_log[16] = "";
    static bool s_last_ident_log     = false;
    if (strcmp(action, s_last_action_log) != 0 || ident != s_last_ident_log) {
        ESP_LOGI(TAG, "[CAM] accion=%s dist=[%d,%d,%d] ident=%d conf=%d%%",
                 action, (int)dist[0], (int)dist[1], (int)dist[2],
                 ident ? 1 : 0, conf);
        snprintf(s_last_action_log, sizeof(s_last_action_log), "%s", action);
        s_last_ident_log = ident;
    }

    cam_result_t res;
    strncpy(res.action, action, sizeof(res.action) - 1);
    res.action[sizeof(res.action) - 1] = '\0';
    for (int c = 0; c < 3; c++) res.dist[c] = dist[c];
    res.ident = ident;
    res.conf  = conf;
    uart_comm_publish(&res);
}

void create_gui() {}
