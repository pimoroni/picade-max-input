#include "pico/stdlib.h"

struct input_t {
    uint16_t p1;
    uint16_t p2;
    uint8_t util;
};

void picade_init();
input_t picade_get_input();