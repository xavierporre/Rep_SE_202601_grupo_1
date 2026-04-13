idf_component_register(
    SRCS "audio.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_adc esp_timer
)
