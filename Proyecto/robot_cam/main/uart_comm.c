#include "uart_comm.h"

#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define UART_PORT       UART_NUM_1
#define UART_TX_GPIO    GPIO_NUM_14   // → ESP32-S3 GPIO18 (RX)
#define UART_RX_GPIO    GPIO_NUM_13   // ← ESP32-S3 GPIO17 (TX), reservado
#define UART_BAUD       115200
#define TX_PERIOD_MS    100           // 10 Hz: independiente de la latencia de inferencia

static const char *TAG = "uart_comm";

static SemaphoreHandle_t s_mutex;
static cam_result_t      s_result = { .action = "stop", .dist = {0, 0, 0},
                                      .ident = false, .conf = 0 };

void uart_comm_publish(const cam_result_t *res) {
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_result = *res;
        s_result.action[sizeof(s_result.action) - 1] = '\0';
        xSemaphoreGive(s_mutex);
    }
}

static void uart_tx_task(void *arg) {
    char line[48];
    while (true) {
        cam_result_t snap;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        snap = s_result;
        xSemaphoreGive(s_mutex);

        int len = snprintf(line, sizeof(line), "D,%s,%d,%d,%d,%d,%d\n",
                           snap.action,
                           (int)snap.dist[0], (int)snap.dist[1], (int)snap.dist[2],
                           snap.ident ? 1 : 0, snap.conf);
        uart_write_bytes(UART_PORT, line, len);

        vTaskDelay(pdMS_TO_TICKS(TX_PERIOD_MS));
    }
}

void uart_comm_init(void) {
    s_mutex = xSemaphoreCreateMutex();

    uart_config_t cfg = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 256, 1024, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_GPIO, UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(uart_tx_task, "uart_tx", 3072, NULL, 6, NULL);
    ESP_LOGI(TAG, "UART1 listo: TX=GPIO%d RX=GPIO%d @ %d baud, TX cada %d ms",
             UART_TX_GPIO, UART_RX_GPIO, UART_BAUD, TX_PERIOD_MS);
}
