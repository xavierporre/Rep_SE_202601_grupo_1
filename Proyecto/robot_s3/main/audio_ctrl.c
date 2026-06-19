#include "audio_ctrl.h"
#include "robot_state.h"
#include "control.h"

#include <math.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_adc/adc_oneshot.h"

#define AUDIO_SAMPLES  256
#define AUDIO_FS       8000    // Hz
#define AUDIO_HOLD_MS  700     // histéresis: mantener dirección si no hay señal

#define MIC_CHANNEL ADC_CHANNEL_0  // GPIO1 (ADC1_CH0)

static adc_oneshot_unit_handle_t s_adc;
static double s_real[AUDIO_SAMPLES];

void audio_ctrl_init(void) {
    adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&init_cfg, &s_adc);

    adc_oneshot_chan_cfg_t ch_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(s_adc, MIC_CHANNEL, &ch_cfg);
}

static void sample_signal(void) {
    int v;
    for (int i = 0; i < AUDIO_SAMPLES; i++) {
        adc_oneshot_read(s_adc, MIC_CHANNEL, &v);
        s_real[i] = v - 2048;
        esp_rom_delay_us(1000000 / AUDIO_FS);
    }
}

static double compute_dft_peak(void) {
    double max_mag = 0;
    int    max_k   = 0;
    for (int k = 1; k < AUDIO_SAMPLES / 2; k++) {
        double re = 0, im = 0;
        for (int n = 0; n < AUDIO_SAMPLES; n++) {
            double angle = 2.0 * M_PI * k * n / AUDIO_SAMPLES;
            re += s_real[n] * cos(angle);
            im -= s_real[n] * sin(angle);
        }
        double mag = sqrt(re * re + im * im);
        if (mag > max_mag) { max_mag = mag; max_k = k; }
    }
    return (double)max_k * AUDIO_FS / AUDIO_SAMPLES;
}

static int band_from_freq(double freq) {
    if (freq > 150 && freq < 450)  return 0;
    if (freq > 550 && freq < 750)  return 1;
    if (freq > 850 && freq < 1000) return 2;
    if (freq > 1050 && freq < 1300) return 3;
    return -1;
}

static void audio_task(void *arg) {
    (void)arg;
    int64_t last_detect_us = esp_timer_get_time();
    bool    motors_on      = false;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1));

        if (control_get_mode() != MODE_AUDIO) {
            if (motors_on) {
                motorStop();
                motors_on = false;
            }
            last_detect_us = esp_timer_get_time();
            continue;
        }

        sample_signal();
        double freq = compute_dft_peak();
        int    band = band_from_freq(freq);

        if (band >= 0) {
            last_detect_us = esp_timer_get_time();
            motors_on = true;
            switch (band) {
                case 0: moveForward(SPEED_LINEAR);  setLED(180, 80, 0); break;
                case 1: turnLeft(SPEED_TURN);        setLED(180, 80, 0); break;
                case 2: turnRight(SPEED_TURN);       setLED(180, 80, 0); break;
                case 3: moveBackward(SPEED_LINEAR);  setLED(180, 80, 0); break;
            }
        } else {
            if ((esp_timer_get_time() - last_detect_us) > (int64_t)AUDIO_HOLD_MS * 1000) {
                if (motors_on) {
                    motorStop();
                    setLED(0, 0, 0);
                    motors_on = false;
                }
            }
        }
    }
}

void audio_ctrl_start(void) {
    audio_ctrl_init();
    xTaskCreate(audio_task, "audio_ctrl", 8192, NULL, 5, NULL);
}
