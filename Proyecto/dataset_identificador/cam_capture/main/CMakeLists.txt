idf_component_register(
    SRCS
        "capture_main.cc"
    PRIV_REQUIRES
        esp32-camera
        esp_driver_gpio
    INCLUDE_DIRS "")
