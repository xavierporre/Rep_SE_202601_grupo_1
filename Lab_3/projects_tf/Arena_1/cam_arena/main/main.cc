#include "main_functions.h"
#include "stream_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_http_client.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "cam_arena";

/* ── WiFi STA ─────────────────────────────────────────────────────────────── */
#define WIFI_SSID    "AutoRC"
#define WIFI_PASS    "12345678"
#define WIFI_RETRIES 10
#define S3_IP        "192.168.4.1"

static EventGroupHandle_t s_wifi_events;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry = 0;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry < WIFI_RETRIES) {
            esp_wifi_connect();
            s_retry++;
            ESP_LOGW(TAG, "Reintentando WiFi (%d/%d)...", s_retry, WIFI_RETRIES);
        } else {
            xEventGroupSetBits(s_wifi_events, WIFI_FAIL_BIT);
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&ev->ip_info.ip));
        s_retry = 0;
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static bool wifi_init_sta(char *ip_out, size_t ip_len)
{
    s_wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t h_any, h_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &h_any));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &h_ip));

    wifi_config_t wifi_cfg = {};
    strncpy((char *)wifi_cfg.sta.ssid,     WIFI_SSID, sizeof(wifi_cfg.sta.ssid));
    strncpy((char *)wifi_cfg.sta.password, WIFI_PASS, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
        pdMS_TO_TICKS(15000));

    if (bits & WIFI_CONNECTED_BIT) {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(netif, &ip_info);
        snprintf(ip_out, ip_len, IPSTR, IP2STR(&ip_info.ip));
        return true;
    }
    ESP_LOGE(TAG, "No se pudo conectar al AP %s", WIFI_SSID);
    return false;
}

static void register_with_s3(const char *my_ip)
{
    char url[64];
    snprintf(url, sizeof(url), "http://" S3_IP "/cam-register?ip=%s", my_ip);
    esp_http_client_config_t cfg = {};
    cfg.url        = url;
    cfg.timeout_ms = 3000;
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
        ESP_LOGI(TAG, "Registrado en S3: %s", url);
    else
        ESP_LOGW(TAG, "Fallo registro en S3: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
}

/* ── Tarea principal: captura + detección de bordes ──────────────────────── */
static void tf_main(void *)
{
    ESP_LOGI(TAG, "Inicializando camara...");
    setup();
    ESP_LOGI(TAG, "Bucle de detección de bordes iniciado");
    while (true) loop();
}

/* ── app_main ────────────────────────────────────────────────────────────── */
extern "C" void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    char my_ip[16] = "";
    bool connected = wifi_init_sta(my_ip, sizeof(my_ip));

    /* Arrancar stream server en puerto 81 (crea mutexes) */
    stream_server_start();

    if (connected) register_with_s3(my_ip);

    xTaskCreate((TaskFunction_t)&tf_main, "tf_main", 32 * 1024, NULL, 8, NULL);
    vTaskDelete(NULL);
}
