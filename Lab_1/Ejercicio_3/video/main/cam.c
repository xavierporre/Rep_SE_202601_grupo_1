#include <stdio.h>
#include <stdlib.h>
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_system.h"

// 🔥 IMPORTANTE: tu placa
#define BOARD_ESP32CAM_AITHINKER

// Pines para ESP32-CAM AI Thinker
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27

#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0      5

#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

static const char *TAG = "CAMERA";

// 🔥 Ecualización de histograma
void histogram_equalization(uint8_t *img, int size) {
    int hist[256] = {0};
    int cdf[256] = {0};

    // Histograma
    for (int i = 0; i < size; i++) {
        hist[img[i]]++;
    }

    // Acumulado
    cdf[0] = hist[0];
    for (int i = 1; i < 256; i++) {
        cdf[i] = cdf[i - 1] + hist[i];
    }

    // Normalización
    for (int i = 0; i < size; i++) {
        img[i] = (cdf[img[i]] * 255) / size;
    }
}

void app_main(void)
{
    camera_config_t config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,

        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        // 🔥 CLAVE DEL LAB
        .pixel_format = PIXFORMAT_GRAYSCALE,
        .frame_size = FRAMESIZE_96X96,

        .jpeg_quality = 12,
        .fb_count = 1,
        // Evita depender de PSRAM (soluciona "frame buffer malloc failed")
        .fb_location = CAMERA_FB_IN_DRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY
    };

    // Inicializar cámara
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error iniciando cámara");
        return;
    }

    ESP_LOGI(TAG, "Cámara iniciada");

    // Capturar imagen
    camera_fb_t *pic = esp_camera_fb_get();
    if (!pic) {
        ESP_LOGE(TAG, "Error capturando imagen");
        return;
    }

    ESP_LOGI(TAG, "Imagen capturada, tamaño: %d bytes", pic->len);

    // 🔥 EJERCICIO 8: ecualización
    histogram_equalization(pic->buf, pic->len);

    // 🔥 SALIDA PARA TXT / COLAB
    printf("DATA_START\n");

    for (int i = 0; i < pic->len; i++) {
        printf("0x%02X,", pic->buf[i]);
    }

    printf("\nDATA_END\n");

    // Liberar buffer
    esp_camera_fb_return(pic);

    ESP_LOGI(TAG, "Proceso terminado");
}