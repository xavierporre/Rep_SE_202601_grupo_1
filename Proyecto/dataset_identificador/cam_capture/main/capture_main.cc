/*
 * cam_capture — firmware standalone para la ESP32-CAM (AI-Thinker, OV2640).
 *
 * Unico proposito: capturar frames 96x96 en escala de grises a pedido y
 * volcarlos por UART0 (el mismo cable USB-TTL usado para flashear) como
 * texto hexadecimal, para construir un dataset de entrenamiento del
 * identificador. No corre ningun modelo TFLite ni toca robot_cam.
 *
 * Protocolo (UART0, 115200 8N1):
 *  - El host envia el caracter 'c' para pedir una captura.
 *  - La placa responde:
 *        ---IMG-START---
 *        <9216*2 caracteres hex, 1 linea>
 *        ---IMG-END---
 *    (9216 = 96*96 bytes, escala de grises, sin convertir a int8 todavia;
 *     el script de captura en el host hace el resto del trabajo)
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "esp_camera.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_FLASH_GPIO GPIO_NUM_4

// Pines AI-Thinker ESP32-CAM (mismos que Proyecto/robot_cam/main/app_camera_esp.h)
#define CAM_PIN_PWDN   32
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD   26
#define CAM_PIN_SIOC   27
#define CAM_PIN_D7     35
#define CAM_PIN_D6     34
#define CAM_PIN_D5     39
#define CAM_PIN_D4     36
#define CAM_PIN_D3     21
#define CAM_PIN_D2     19
#define CAM_PIN_D1     18
#define CAM_PIN_D0      5
#define CAM_PIN_VSYNC  25
#define CAM_PIN_HREF   23
#define CAM_PIN_PCLK   22

static const char *TAG = "cam_capture";

static int camera_init(void) {
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0  = CAM_PIN_D0;
    config.pin_d1  = CAM_PIN_D1;
    config.pin_d2  = CAM_PIN_D2;
    config.pin_d3  = CAM_PIN_D3;
    config.pin_d4  = CAM_PIN_D4;
    config.pin_d5  = CAM_PIN_D5;
    config.pin_d6  = CAM_PIN_D6;
    config.pin_d7  = CAM_PIN_D7;
    config.pin_xclk  = CAM_PIN_XCLK;
    config.pin_pclk  = CAM_PIN_PCLK;
    config.pin_vsync = CAM_PIN_VSYNC;
    config.pin_href  = CAM_PIN_HREF;
    config.pin_sscb_sda = CAM_PIN_SIOD;
    config.pin_sscb_scl = CAM_PIN_SIOC;
    config.pin_pwdn  = CAM_PIN_PWDN;
    config.pin_reset = CAM_PIN_RESET;
    config.xclk_freq_hz = 15000000;
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.frame_size   = FRAMESIZE_96X96;
    config.jpeg_quality = 10;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return -1;
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);
    if (s->id.PID == OV3660_PID) {
        s->set_brightness(s, 1);
        s->set_saturation(s, -2);
    }
    return 0;
}

static void capture_task(void *) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << LED_FLASH_GPIO);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    gpio_set_level(LED_FLASH_GPIO, 0);

    if (camera_init() != 0) {
        ESP_LOGE(TAG, "No se pudo inicializar la camara, abortando.");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Camara lista (96x96 escala de grises). Envia 'c' por UART0 para capturar.");

    // Lectura no bloqueante de stdin (consola UART0).
    fcntl(fileno(stdin), F_SETFL, fcntl(fileno(stdin), F_GETFL) | O_NONBLOCK);

    static const char hex_digits[] = "0123456789abcdef";
    static char linebuf[2048];

    while (true) {
        char c;
        int n = read(fileno(stdin), &c, 1);
        if (n == 1 && c == 'c') {
            gpio_set_level(LED_FLASH_GPIO, 1);

            // El driver esp32-camera sigue llenando el ring buffer (fb_count=2)
            // en segundo plano mientras esperamos el comando: esp_camera_fb_get()
            // devuelve el frame mas viejo en cola, no el actual. Se descartan
            // los frames atrasados antes de tomar el que realmente se va a usar.
            for (int i = 0; i < 2; i++) {
                camera_fb_t *stale = esp_camera_fb_get();
                if (stale) esp_camera_fb_return(stale);
            }

            camera_fb_t *fb = esp_camera_fb_get();
            if (!fb) {
                printf("ERR:capture_failed\n");
            } else {
                printf("---IMG-START---\n");
                size_t pos = 0;
                for (size_t i = 0; i < fb->len; i++) {
                    linebuf[pos++] = hex_digits[(fb->buf[i] >> 4) & 0xF];
                    linebuf[pos++] = hex_digits[fb->buf[i] & 0xF];
                    if (pos >= sizeof(linebuf) - 1) {
                        fwrite(linebuf, 1, pos, stdout);
                        pos = 0;
                    }
                }
                if (pos > 0) fwrite(linebuf, 1, pos, stdout);
                printf("\n---IMG-END---\n");
                fflush(stdout);
                esp_camera_fb_return(fb);
            }
            gpio_set_level(LED_FLASH_GPIO, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

extern "C" void app_main(void) {
    xTaskCreate(capture_task, "capture_task", 16 * 1024, NULL, 5, NULL);
}
