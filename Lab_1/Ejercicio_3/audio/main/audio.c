#include <stdio.h>
#include <math.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_rom_sys.h"

#define SAMPLES 256
#define SAMPLING_FREQUENCY 8000

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
        sample_signal();

        double freq = compute_dft_peak();

        printf("Frecuencia: %.2f Hz\n", freq);

        apagar_todos();

        if (freq > 450 && freq < 550) {
            gpio_set_level(LED1, 1);
        }
        else if (freq > 950 && freq < 1050) {
            gpio_set_level(LED2, 1);
        }
        else if (freq > 1900 && freq < 2100) {
            gpio_set_level(LED3, 1);
        }
        else if (freq > 2900 && freq < 3100) {
            gpio_set_level(LED4, 1);
        }
    }
}