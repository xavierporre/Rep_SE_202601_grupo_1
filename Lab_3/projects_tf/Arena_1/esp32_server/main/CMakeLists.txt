idf_component_register(
    SRCS "web.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_http_client esp_http_server esp_timer esp_wifi esp_netif nvs_flash esp_event esp_driver_gpio esp_driver_ledc
)