#include "pico/stdlib.h"

const uint PLASMA_CLOCK = 22;
const uint PLASMA_DATA = 23;

void plasma_init();
void plasma_set_all(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness=31);