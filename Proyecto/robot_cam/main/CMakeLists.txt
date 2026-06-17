idf_component_register(
    SRCS
        "main.cc"
        "main_functions.cc"
        "model_settings.cc"
        "identificador_model_data.cc"
        "image_provider.cc"
        "detection_responder.cc"
        "app_camera_esp.c"
        "uart_comm.c"
    PRIV_REQUIRES
        esp32-camera
        spi_flash
        esp_driver_gpio
        esp_driver_uart
    INCLUDE_DIRS "")
