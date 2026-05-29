// Bridge file: includes group photo data directly and exposes them as extern "C"
// for use in esp_cli.c (C++ const globals have internal linkage, hence the bridge)

#include "../../../fotos/person_image_1.cc"
#include "../../../fotos/person_image_2.cc"
#include "../../../fotos/person_image_3.cc"

extern "C" {
    const int8_t *get_person_image_1(void) { return person_image_1_data; }
    const int8_t *get_person_image_2(void) { return person_image_2_data; }
    const int8_t *get_person_image_3(void) { return person_image_3_data; }
}
