#include <stdio.h>
#include <math.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SAMPLES 256
#define SAMPLING_FREQUENCY 8000

// Duración objetivo por iteración del loop principal
#define ITERATION_MS 30000

// Mantén el LED encendido aunque haya ventanas sin detección
#define LED_HOLD_MS 700

#define MIC_CHANNEL ADC_CHANNEL_1  // GPIO1 (ajusta si usas otro)
#define LED1 GPIO_NUM_10
#define LED2 GPIO_NUM_11
#define LED3 GPIO_NUM_12
#define LED4 GPIO_NUM_13

double vReal[SAMPLES];
double vImag[SAMPLES];

adc_oneshot_unit_handle_t adc_handle;

// 🔧 Inicializar ADC
void init_adc() {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config, &adc_handle);

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12
    };

    adc_oneshot_config_channel(adc_handle, MIC_CHANNEL, &config);
}

// 🔧 Inicializar LEDs
void init_leds() {
    gpio_reset_pin(LED1);
    gpio_reset_pin(LED2);
    gpio_reset_pin(LED3);
    gpio_reset_pin(LED4);

    gpio_set_direction(LED1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED3, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED4, GPIO_MODE_OUTPUT);
}

// 🔴 apagar LEDs
void apagar_todos() {
    gpio_set_level(LED1, 0);
    gpio_set_level(LED2, 0);
    gpio_set_level(LED3, 0);
    gpio_set_level(LED4, 0);
}

static int band_from_freq(double freq) {
    if (freq > 150 && freq < 450) {
        return 0;
    }
    if (freq > 550 && freq < 750) {
        return 1;
    }
    if (freq > 850 && freq < 1000) {
        return 2;
    }
    if (freq > 1050 && freq < 1300) {
        return 3;
    }
    return -1;
}

static void set_led_band(int band) {
    apagar_todos();
    switch (band) {
        case 0:
            gpio_set_level(LED1, 1);
            break;
        case 1:
            gpio_set_level(LED2, 1);
            break;
        case 2:
            gpio_set_level(LED3, 1);
            break;
        case 3:
            gpio_set_level(LED4, 1);
            break;
        default:
            break;
    }
}

// 📥 Capturar señal
void sample_signal() {
    int value;
    for (int i = 0; i < SAMPLES; i++) {
        adc_oneshot_read(adc_handle, MIC_CHANNEL, &value);
        vReal[i] = value - 2048; // centrar
        vImag[i] = 0;

        esp_rom_delay_us(1000000 / SAMPLING_FREQUENCY);
    }
}

// 🧠 DFT simple (lenta pero clara)
double compute_dft_peak() {
    double max_mag = 0;
    int max_index = 0;

    for (int k = 1; k < SAMPLES / 2; k++) {
        double real = 0;
        double imag = 0;

        for (int n = 0; n < SAMPLES; n++) {
            double angle = 2 * M_PI * k * n / SAMPLES;
            real += vReal[n] * cos(angle);
            imag -= vReal[n] * sin(angle);
        }

        double magnitude = sqrt(real * real + imag * imag);

        if (magnitude > max_mag) {
            max_mag = magnitude;
            max_index = k;
        }
    }

    double freq = (max_index * SAMPLING_FREQUENCY) / SAMPLES;
    return freq;
}

void app_main() {
    init_adc();
    init_leds();

    while (1) {
        int counts[4] = {0, 0, 0, 0};
        int best_band = -1;
        int best_count = 0;

        int64_t start_us = esp_timer_get_time();
        int64_t last_detect_us = start_us;
        int64_t last_print_us = start_us;

        while ((esp_timer_get_time() - start_us) < (int64_t)ITERATION_MS * 1000) {
            sample_signal();
            double freq = compute_dft_peak();
            int band = band_from_freq(freq);

            if (band >= 0) {
                counts[band]++;
                last_detect_us = esp_timer_get_time();

                if (counts[band] > best_count) {
                    best_count = counts[band];
                    best_band = band;
                }

                set_led_band(band);
            } else {
                if ((esp_timer_get_time() - last_detect_us) > (int64_t)LED_HOLD_MS * 1000) {
                    apagar_todos();
                }
            }

            // Print de estado cada ~1s para no saturar el monitor
            if ((esp_timer_get_time() - last_print_us) > 1000000) {
                printf("Freq: %.2f Hz | c0=%d c1=%d c2=%d c3=%d\n",
                       freq, counts[0], counts[1], counts[2], counts[3]);
                last_print_us = esp_timer_get_time();
            }

            // Cede CPU (evita watchdog en bucles largos)
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        printf("Resumen %dms | c0=%d c1=%d c2=%d c3=%d | dominante=%d\n",
               ITERATION_MS, counts[0], counts[1], counts[2], counts[3], best_band);

        if (best_band >= 0) {
            set_led_band(best_band);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        apagar_todos();
    }
}