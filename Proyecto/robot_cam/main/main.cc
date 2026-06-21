/*
 * robot_cam — ESP32-CAM (AI-Thinker, OV2640)
 *
 * Captura frames 96x96 en escala de grises y, sobre el mismo frame:
 *  - clasifica el identificador "embutido" con TFLite Micro
 *  - detecta el borde negro del ring con la grilla 3x2
 * El resultado se envia al ESP32-S3 por UART1 (GPIO14 TX / GPIO13 RX).
 * Esta placa NO usa WiFi.
 */

#include "main_functions.h"
#include "uart_comm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED_FLASH_GPIO GPIO_NUM_4

static const char *TAG = "robot_cam";

static void tf_main(void *) {
    // Configurar GPIO 4 como salida (LED flash AI Thinker)
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << LED_FLASH_GPIO);
    io_conf.mode         = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(LED_FLASH_GPIO, 0);  // LED apagado al inicio

    uart_comm_init();

    ESP_LOGI(TAG, "Inicializando modelo TFLite y camara OV2640...");
    setup();
    ESP_LOGI(TAG, "Bucle camara: borde (grilla 3x2) + identificador (umbral >= 50%%, EMA=0.60, cada 5 frames)");
    while (true) {
        loop();
    }
}

extern "C" void app_main() {
    xTaskCreate((TaskFunction_t)&tf_main, "tf_main", 32 * 1024, NULL, 8, NULL);
    vTaskDelete(NULL);
}
