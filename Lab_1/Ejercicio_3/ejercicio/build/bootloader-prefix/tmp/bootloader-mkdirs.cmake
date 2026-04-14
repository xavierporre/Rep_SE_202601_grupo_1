# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/ap6d2/esp/idf/esp-idf/components/bootloader/subproject"
  "/home/ap6d2/esp/projects/Rep_SE_202601_grupo_1/Lab_1/Ejercicio_3/ejercicio/build/bootloader"
  "/home/ap6d2/esp/projects/Rep_SE_202601_grupo_1/Lab_1/Ejercicio_3/ejercicio/build/bootloader-prefix"
  "/home/ap6d2/esp/projects/Rep_SE_202601_grupo_1/Lab_1/Ejercicio_3/ejercicio/build/bootloader-prefix/tmp"
  "/home/ap6d2/esp/projects/Rep_SE_202601_grupo_1/Lab_1/Ejercicio_3/ejercicio/build/bootloader-prefix/src/bootloader-stamp"
  "/home/ap6d2/esp/projects/Rep_SE_202601_grupo_1/Lab_1/Ejercicio_3/ejercicio/build/bootloader-prefix/src"
  "/home/ap6d2/esp/projects/Rep_SE_202601_grupo_1/Lab_1/Ejercicio_3/ejercicio/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/ap6d2/esp/projects/Rep_SE_202601_grupo_1/Lab_1/Ejercicio_3/ejercicio/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/ap6d2/esp/projects/Rep_SE_202601_grupo_1/Lab_1/Ejercicio_3/ejercicio/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
