
#
# Main component of TF Micro project 'person_detection'.
#

idf_component_register(
    SRCS
        "detection_responder.cc"
        "image_provider.cc"
        "main.cc"
        "main_functions.cc"
        "model_settings.cc"
        "person_detect_model_data.cc"
        "esp_cli.c"
        "person_images_bridge.cc"

    PRIV_REQUIRES console static_images spi_flash
    INCLUDE_DIRS "")
