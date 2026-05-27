cmake_minimum_required(VERSION 3.5)

# Get the absolute path to Lab_3 (two directories up: hello_world -> projects_tf -> Lab_3)
get_filename_component(LAB3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Register local esp-tflite-micro as extra component directory
set(TFLITE_COMPONENT_DIR "${LAB3_DIR}/esp-tflite-micro")
message(STATUS "Lab_3 directory: ${LAB3_DIR}")
message(STATUS "ESP-TFLite Micro component directory: ${TFLITE_COMPONENT_DIR}")

# Verify the directory exists
if(NOT EXISTS "${TFLITE_COMPONENT_DIR}")
    message(FATAL_ERROR "ESP-TFLite Micro component not found at: ${TFLITE_COMPONENT_DIR}")
endif()

list(APPEND EXTRA_COMPONENT_DIRS "${TFLITE_COMPONENT_DIR}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello_world)
