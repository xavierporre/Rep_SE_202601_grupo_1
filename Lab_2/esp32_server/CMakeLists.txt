cmake_minimum_required(VERSION 3.16)
 
# Incluye el soporte Arduino dentro de ESP-IDF
set(ARDUINO_VERSION "2.0.17")
 
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_server)