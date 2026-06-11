#include "uart_link.h"

#include <cstring>
#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#define UART_PORT     UART_NUM_1
#define UART_TX_GPIO  GPIO_NUM_17   // → ESP32-CAM GPIO13 (reservado)
#define UART_RX_GPIO  GPIO_NUM_18   // ← ESP32-CAM GPIO14
#define UART_BAUD     115200

static const char *TAG = "uart_link";

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static cam_data_t   s_data;
static uint32_t     s_last_ms = 0;
static bool         s_have    = false;

static inline uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

bool uart_link_get(cam_data_t *out, uint32_t *age_ms) {
    portENTER_CRITICAL(&s_lock);
    bool have = s_have;
    if (have) {
        *out = s_data;
        *age_ms = millis() - s_last_ms;
    }
    portEXIT_CRITICAL(&s_lock);
    return have;
}

// Valida y guarda una linea "D,<action>,<d0>,<d1>,<d2>,<ident>,<conf>"
static void parse_line(const char *line) {
    char action[16] = {0};
    int  d0, d1, d2, id, conf;

    if (sscanf(line, "D,%15[^,],%d,%d,%d,%d,%d",
               action, &d0, &d1, &d2, &id, &conf) != 6) return;
    if (d0 < 0 || d0 > 2 || d1 < 0 || d1 > 2 || d2 < 0 || d2 > 2) return;
    if (id < 0 || id > 1 || conf < 0 || conf > 100) return;
    if (strcmp(action, "forward")    != 0 &&
        strcmp(action, "turn_left")  != 0 &&
        strcmp(action, "turn_right") != 0 &&
        strcmp(action, "stop")       != 0) return;

    portENTER_CRITICAL(&s_lock);
    strncpy(s_data.action, action, sizeof(s_data.action) - 1);
    s_data.action[sizeof(s_data.action) - 1] = '\0';
    s_data.dist[0] = (int8_t)d0;
    s_data.dist[1] = (int8_t)d1;
    s_data.dist[2] = (int8_t)d2;
    s_data.ident   = (id == 1);
    s_data.conf    = conf;
    s_last_ms      = millis();
    s_have         = true;
    portEXIT_CRITICAL(&s_lock);
}

static void uart_rx_task(void *) {
    uint8_t buf[64];
    char    line[96];
    size_t  pos = 0;

    while (true) {
        int n = uart_read_bytes(UART_PORT, buf, sizeof(buf), pdMS_TO_TICKS(20));
        for (int i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\n') {
                line[pos] = '\0';
                if (pos > 0) parse_line(line);
                pos = 0;
            } else if (c == '\r') {
                // ignorar
            } else if (pos < sizeof(line) - 1) {
                line[pos++] = c;
            } else {
                pos = 0;  // desborde sin newline: descartar el buffer
            }
        }
    }
}

void uart_link_init(void) {
    uart_config_t cfg = {};
    cfg.baud_rate  = UART_BAUD;
    cfg.data_bits  = UART_DATA_8_BITS;
    cfg.parity     = UART_PARITY_DISABLE;
    cfg.stop_bits  = UART_STOP_BITS_1;
    cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_GPIO, UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "UART1 listo: TX=GPIO%d RX=GPIO%d @ %d baud",
             UART_TX_GPIO, UART_RX_GPIO, UART_BAUD);
}
