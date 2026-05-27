cmake_minimum_required(VERSION 3.5)

# Get the absolute path to Lab_3 (two directories up: person_detection -> projects_tf -> Lab_3)
get_filename_component(LAB3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Register local esp-tflite-micro and static_images as extra component directories
set(TFLITE_COMPONENT_DIR "${LAB3_DIR}/esp-tflite-micro")
set(STATIC_IMAGES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/static_images")
message(STATUS "Lab_3 directory: ${LAB3_DIR}")
message(STATUS "ESP-TFLite Micro component directory: ${TFLITE_COMPONENT_DIR}")
message(STATUS "Static images directory: ${STATIC_IMAGES_DIR}")

# Verify directories exist
if(NOT EXISTS "${TFLITE_COMPONENT_DIR}")
    message(FATAL_ERROR "ESP-TFLite Micro component not found at: ${TFLITE_COMPONENT_DIR}")
endif()

if(NOT EXISTS "${STATIC_IMAGES_DIR}")
    message(FATAL_ERROR "Static images directory not found at: ${STATIC_IMAGES_DIR}")
endif()

list(APPEND EXTRA_COMPONENT_DIRS "${TFLITE_COMPONENT_DIR}")
list(APPEND EXTRA_COMPONENT_DIRS "${STATIC_IMAGES_DIR}")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Helper function to set required environment variables as per configuration set by user
function(add_bsp SDKCONFIG BSP)
    string(REGEX MATCH "CONFIG_${BSP}=y" REGEX_RESULT ${SDKCONFIG})
    if (REGEX_RESULT)
        set(ENV{${BSP}} 1) # If the config option is set then set corresponding environment variable to 1
    endif()
endfunction()

# 1. Define all variables used in main/idf_component.yml and set them to zero
set(ENV{TFLITE_USE_BSP_S3_EYE} 0)
set(ENV{TFLITE_USE_BSP_KORVO_2} 0)
set(ENV{TFLITE_USE_BSP_KALUGA} 0)

# 2. Set correct var to 1
# This is a workaround idf-component-manager limitation, where only
# target, idf_version and environment variables can be in the if-clause
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/sdkconfig)
    file(READ ${CMAKE_CURRENT_LIST_DIR}/sdkconfig SDKCONFIG_RULE)

    add_bsp("${SDKCONFIG_RULE}" "TFLITE_USE_BSP_S3_EYE") # Check for CONFIG_TFLITE_USE_BSP_S3_EYE option in sdkconfig
    add_bsp("${SDKCONFIG_RULE}" "TFLITE_USE_BSP_KORVO_2") # Check for CONFIG_TFLITE_USE_BSP_KORVO_2 option in sdkconfig
    add_bsp("${SDKCONFIG_RULE}" "TFLITE_USE_BSP_KALUGA") # Check for CONFIG_TFLITE_USE_BSP_KALUGA option in sdkconfig
endif()

project(person_detection)
