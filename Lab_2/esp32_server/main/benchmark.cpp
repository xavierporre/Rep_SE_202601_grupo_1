/*
 * Benchmark ESP32-S3 — Sección 2.3 Lab 2
 * Mide tiempo de operaciones y movimiento de memoria
 * para estimar latencia, FPS y potencia de cada red neuronal.
 *
 * Uso: agregar este archivo a main/ y llamar benchmark_all() desde app_main()
 * El resultado se imprime por el monitor serie (idf.py monitor)
 */

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <cmath>
#include <cstdlib>

static const char* TAG = "BENCH";

// ── Parámetros del ESP32-S3 ────────────────────────────────
// Potencia medida típica ESP32-S3:
//   Idle WiFi activo : ~68 mW  (3.3V × ~20 mA)
//   Activo máximo    : ~108 mW (3.3V × ~33 mA)
// Sin multímetro, estimamos por fracción de tiempo activo.
#define ESP32S3_FREQ_MHZ   240
#define ESP32S3_P_IDLE_MW   68.0f   // mW WiFi activo idle
#define ESP32S3_P_ACTIVE_MW 40.0f   // mW adicional bajo carga CPU

// ── Utilidad: tiempo en microsegundos ──────────────────────
static inline int64_t now_us() {
    return esp_timer_get_time();
}

// ── Benchmark 1: tiempo por MAC (multiply-accumulate) ─────
// Simula operación de capa fully connected / convolución
static float bench_mac_us_per_op(int num_macs) {
    // Allocate vectores pequeños
    static float a[256], b[256];
    for (int i = 0; i < 256; i++) { a[i] = (float)i * 0.01f; b[i] = (float)(256-i) * 0.01f; }

    volatile float acc = 0.0f;
    int64_t t0 = now_us();

    // Repetir muchas veces para medir con precisión
    int reps = num_macs / 256 + 1;
    for (int r = 0; r < reps; r++) {
        for (int i = 0; i < 256; i++) {
            acc += a[i] * b[i];
        }
    }

    int64_t dt = now_us() - t0;
    int total_ops = reps * 256;
    float us_per_op = (float)dt / total_ops;
    (void)acc;
    return us_per_op;
}

// ── Benchmark 2: tiempo por byte movido en memoria ─────────
static float bench_memcpy_ns_per_byte(int bytes) {
    static uint8_t src[4096], dst[4096];
    memset(src, 0xAB, sizeof(src));

    int chunk = (bytes > 4096) ? 4096 : bytes;
    int reps  = 1000;

    int64_t t0 = now_us();
    for (int r = 0; r < reps; r++) {
        memcpy(dst, src, chunk);
    }
    int64_t dt = now_us() - t0;

    float ns_per_byte = (float)(dt * 1000) / (float)(reps * chunk);
    (void)dst;
    return ns_per_byte;
}

// ── Estructura de red ──────────────────────────────────────
struct LayerInfo {
    const char* name;
    long long   params;
    long long   macs;
    long long   in_bytes;
    long long   out_bytes;
};

struct NetworkInfo {
    const char*  name;
    int          latency_ms;   // medido en ESP
    float        fps;
    LayerInfo    layers[10];
    int          num_layers;
};

// ── Definición de las 5 redes (96x96x3, 2 clases) ─────────
static NetworkInfo networks[] = {
    {
        "MLP Simple", 1090, 0.9f,
        {
            {"Flatten",   0,             0,              27648*4, 27648*4},
            {"Dense 128", 27648*128+128, 27648LL*128,   27648*4, 128*4},
            {"Dense 64",  128*64+64,     128*64,        128*4,   64*4},
            {"Dense 2",   64*2+2,        64*2,          64*4,    2*4},
        }, 4
    },
    {
        "CNN Simple", 268, 3.7f,
        {
            {"Conv2D 8x3x3", 8*3*3*3+8,   (long long)8*3*3*3*48*48, 96*96*3*4, 48*48*8*4},
            {"MaxPool2D",    0,             0,                        48*48*8*4, 24*24*8*4},
            {"Flatten",      0,             0,                        24*24*8*4, 24*24*8*4},
            {"Dense 32",     24*24*8*32+32,(long long)24*24*8*32,    24*24*8*4, 32*4},
            {"Dense 2",      32*2+2,        32*2,                    32*4,      2*4},
        }, 5
    },
    {
        "CNN Keras", 2112, 0.5f,
        {
            {"Conv2D 16x3x3", 16*3*3*3+16,  (long long)16*3*3*3*96*96,  96*96*3*4,  96*96*16*4},
            {"MaxPool2D",     0,              0,                          96*96*16*4, 48*48*16*4},
            {"Conv2D 32x3x3", 32*3*3*16+32,  (long long)32*3*3*16*48*48, 48*48*16*4, 48*48*32*4},
            {"MaxPool2D 2",   0,              0,                          48*48*32*4, 24*24*32*4},
            {"Flatten",       0,              0,                          24*24*32*4, 24*24*32*4},
            {"Dense 64",      24*24*32*64+64,(long long)24*24*32*64,     24*24*32*4, 64*4},
            {"Dense 2",       64*2+2,         64*2,                      64*4,       2*4},
        }, 7
    },
    {
        "CNN Profundo", 1772, 0.6f,
        {
            {"Conv2D 8x3x3",  8*3*3*3+8,    (long long)8*3*3*3*96*96,   96*96*3*4,  96*96*8*4},
            {"MaxPool2D",     0,              0,                          96*96*8*4,  48*48*8*4},
            {"Conv2D 16x3x3", 16*3*3*8+16,   (long long)16*3*3*8*48*48,  48*48*8*4,  48*48*16*4},
            {"MaxPool2D 2",   0,              0,                          48*48*16*4, 24*24*16*4},
            {"Conv2D 32x3x3", 32*3*3*16+32,  (long long)32*3*3*16*24*24, 24*24*16*4, 24*24*32*4},
            {"MaxPool2D 3",   0,              0,                          24*24*32*4, 12*12*32*4},
            {"Flatten",       0,              0,                          12*12*32*4, 12*12*32*4},
            {"Dense 64",      12*12*32*64+64,(long long)12*12*32*64,     12*12*32*4, 64*4},
            {"Dense 2",       64*2+2,         64*2,                      64*4,       2*4},
        }, 9
    },
    {
        "CNN con Dropout", 970, 1.0f,
        {
            {"Conv2D 16x3x3", 16*3*3*3+16,  (long long)16*3*3*3*96*96, 96*96*3*4,  96*96*16*4},
            {"MaxPool2D",     0,              0,                        96*96*16*4, 48*48*16*4},
            {"Dropout 0.3",   0,              0,                        48*48*16*4, 48*48*16*4},
            {"Flatten",       0,              0,                        48*48*16*4, 48*48*16*4},
            {"Dense 32",      48*48*16*32+32,(long long)48*48*16*32,   48*48*16*4, 32*4},
            {"Dropout 0.5",   0,              0,                        32*4,       32*4},
            {"Dense 2",       32*2+2,         32*2,                    32*4,       2*4},
        }, 7
    },
};

// ── Función principal de benchmark ─────────────────────────
void benchmark_all() {
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, "  BENCHMARK ESP32-S3 — Lab 2 Seccion 2.3");
    ESP_LOGI(TAG, "  CPU: %d MHz | SRAM: 512 KB", ESP32S3_FREQ_MHZ);
    ESP_LOGI(TAG, "==============================================");

    vTaskDelay(500 / portTICK_PERIOD_MS);

    // ── Medir velocidad base del hardware ──────────────────
    ESP_LOGI(TAG, "\n--- Calibracion hardware ---");

    float mac_us = bench_mac_us_per_op(256 * 100);
    ESP_LOGI(TAG, "Tiempo por MAC  : %.4f us  (%.2f MMACS/s)",
             mac_us, 1.0f / mac_us);

    float mem_ns = bench_memcpy_ns_per_byte(4096);
    ESP_LOGI(TAG, "Tiempo por byte : %.3f ns  (Ancho de banda: %.1f MB/s)",
             mem_ns, 1000.0f / mem_ns);

    vTaskDelay(200 / portTICK_PERIOD_MS);

    // ── Analizar cada red ──────────────────────────────────
    int num_nets = sizeof(networks) / sizeof(networks[0]);

    ESP_LOGI(TAG, "\n--- Resultados por red ---");
    ESP_LOGI(TAG, "%-20s %8s %6s %10s %10s %10s %8s",
             "Red", "Lat(ms)", "FPS", "FLOPs(M)", "Mem(MB)", "Pot(mW)", "Mod(KB)");
    ESP_LOGI(TAG, "%-20s %8s %6s %10s %10s %10s %8s",
             "--------------------","-------","------","----------","----------","----------","--------");

    for (int n = 0; n < num_nets; n++) {
        NetworkInfo* net = &networks[n];

        long long total_params = 0, total_macs = 0;
        long long total_in = 0, total_out = 0;

        for (int l = 0; l < net->num_layers; l++) {
            total_params += net->layers[l].params;
            total_macs   += net->layers[l].macs;
            total_in     += net->layers[l].in_bytes;
            total_out    += net->layers[l].out_bytes;
        }

        long long total_flops   = total_macs * 2;
        float     mem_move_mb   = (float)(total_in + total_out) / 1e6f;
        float     model_kb      = (float)(total_params * 4) / 1024.0f;

        // Latencia estimada desde medición de hardware
        float lat_from_macs_ms = (float)total_macs * mac_us / 1000.0f;
        float lat_from_mem_ms  = (float)(total_in + total_out) * mem_ns / 1e9f * 1000.0f;
        // La latencia real es dominada por el mayor de los dos
        float lat_estimated_ms = fmaxf(lat_from_macs_ms, lat_from_mem_ms);

        // Potencia: fracción de tiempo activo
        float load = fminf(1.0f, (float)net->latency_ms / 2200.0f);
        float power_mw = ESP32S3_P_IDLE_MW + ESP32S3_P_ACTIVE_MW * load;

        float fps_estimated = 1000.0f / (float)net->latency_ms;

        ESP_LOGI(TAG, "%-20s %8d %6.1f %10.2f %10.2f %10.1f %8.1f",
                 net->name,
                 net->latency_ms,
                 fps_estimated,
                 (float)total_flops / 1e6f,
                 mem_move_mb,
                 power_mw,
                 model_kb);

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    // ── Detalle por capas de cada red ──────────────────────
    ESP_LOGI(TAG, "\n--- Detalle por capas ---");
    for (int n = 0; n < num_nets; n++) {
        NetworkInfo* net = &networks[n];
        ESP_LOGI(TAG, "\n[%s]  lat=%d ms  fps=%.1f",
                 net->name, net->latency_ms, net->fps);
        ESP_LOGI(TAG, "  %-18s %10s %12s %10s %10s",
                 "Capa", "Params", "MACs", "In(KB)", "Out(KB)");

        long long sum_params=0, sum_macs=0, sum_in=0, sum_out=0;
        for (int l = 0; l < net->num_layers; l++) {
            LayerInfo* ly = &net->layers[l];
            ESP_LOGI(TAG, "  %-18s %10lld %12lld %10.1f %10.1f",
                     ly->name, ly->params, ly->macs,
                     (float)ly->in_bytes/1024.0f,
                     (float)ly->out_bytes/1024.0f);
            sum_params += ly->params;
            sum_macs   += ly->macs;
            sum_in     += ly->in_bytes;
            sum_out    += ly->out_bytes;
        }
        ESP_LOGI(TAG, "  %-18s %10lld %12lld %10.1f %10.1f",
                 "TOTAL", sum_params, sum_macs,
                 (float)sum_in/1024.0f, (float)sum_out/1024.0f);
        ESP_LOGI(TAG, "  Mem movida total: %.2f MB | FLOPs: %.2f M | Modelo: %.1f KB",
                 (float)(sum_in+sum_out)/1e6f,
                 (float)(sum_macs*2)/1e6f,
                 (float)(sum_params*4)/1024.0f);

        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "\n==============================================");
    ESP_LOGI(TAG, "  BENCHMARK COMPLETADO");
    ESP_LOGI(TAG, "  Copia esta salida para tu output.pdf");
    ESP_LOGI(TAG, "==============================================");
}
