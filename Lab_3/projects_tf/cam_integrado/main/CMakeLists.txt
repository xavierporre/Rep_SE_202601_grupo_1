idf_component_register(
    SRCS
        "main.cc"
        "main_functions.cc"
        "model_settings.cc"
        "person_detect_model_data.cc"
        "image_provider.cc"
        "detection_responder.cc"
        "app_camera_esp.c"
    PRIV_REQUIRES
        esp32-camera
        spi_flash
        esp_driver_gpio
    INCLUDE_DIRS "")
