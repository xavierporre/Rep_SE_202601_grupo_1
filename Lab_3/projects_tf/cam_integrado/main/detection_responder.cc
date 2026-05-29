#include "detection_responder.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define LED_FLASH_GPIO    GPIO_NUM_4
#define PERSON_THRESHOLD  0.75f

static const char *TAG = "detection";

void RespondToDetection(float person_score, float no_person_score) {
    int person_pct    = (int)(person_score    * 100 + 0.5f);
    int no_person_pct = (int)(no_person_score * 100 + 0.5f);

    if (person_score >= PERSON_THRESHOLD) {
        gpio_set_level(LED_FLASH_GPIO, 1);
        ESP_LOGI(TAG, "[LED ON ] Persona detectada  | persona=%d%%  no_persona=%d%%",
                 person_pct, no_person_pct);
    } else {
        gpio_set_level(LED_FLASH_GPIO, 0);
        ESP_LOGI(TAG, "[LED OFF] Sin persona        | persona=%d%%  no_persona=%d%%",
                 person_pct, no_person_pct);
    }
}

// Stub requerido por detection_responder.h (sin display)
void create_gui() {}
