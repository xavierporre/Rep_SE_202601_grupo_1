/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "main_functions.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdint>

#include "esp_main.h"
#include "esp_cli.h"

static const char *TAG = "main";

// Imagenes estaticas (definidas en el componente static_images)
extern const uint8_t image0_start[] asm("_binary_image0_start");
extern const uint8_t image1_start[] asm("_binary_image1_start");
extern const uint8_t image2_start[] asm("_binary_image2_start");
extern const uint8_t image3_start[] asm("_binary_image3_start");
extern const uint8_t image4_start[] asm("_binary_image4_start");
extern const uint8_t image5_start[] asm("_binary_image5_start");
extern const uint8_t image6_start[] asm("_binary_image6_start");
extern const uint8_t image7_start[] asm("_binary_image7_start");
extern const uint8_t image8_start[] asm("_binary_image8_start");
extern const uint8_t image9_start[] asm("_binary_image9_start");

// Fotos del grupo (via bridge)
extern "C" {
  const int8_t *get_person_image_1(void);
  const int8_t *get_person_image_2(void);
  const int8_t *get_person_image_3(void);
}

void tf_main(void) {
  setup();

#if CLI_ONLY_INFERENCE
  // Inferencia automatica sobre todas las imagenes al arrancar
  const uint8_t *static_images[] = {
    image0_start, image1_start, image2_start, image3_start, image4_start,
    image5_start, image6_start, image7_start, image8_start, image9_start,
  };

  ESP_LOGI(TAG, "=== Inferencia automatica: imagenes estaticas ===");
  for (int i = 0; i < 10; i++) {
    ESP_LOGI(TAG, "--- Imagen estatica %d ---", i);
    run_inference((void *) static_images[i]);
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  ESP_LOGI(TAG, "=== Inferencia automatica: fotos del grupo ===");
  const int8_t *group_images[] = {
    get_person_image_1(),
    get_person_image_2(),
    get_person_image_3(),
  };
  const char *names[] = {"Xavier", "Martin", "Vicente"};
  for (int i = 0; i < 3; i++) {
    ESP_LOGI(TAG, "--- Foto integrante %d (%s) ---", i + 1, names[i]);
    run_inference_int8(group_images[i]);
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  ESP_LOGI(TAG, "=== Inferencia completada ===");
  vTaskDelay(portMAX_DELAY);
#else
  while (true) {
    loop();
  }
#endif
}

extern "C" void app_main() {
  xTaskCreate((TaskFunction_t)&tf_main, "tf_main", 4 * 1024, NULL, 8, NULL);
  vTaskDelete(NULL);
}
