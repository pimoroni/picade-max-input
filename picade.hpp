#include "pico/stdlib.h"

const int16_t JOYSTICK_LEFT  = 0b1000000000000000;
const int16_t JOYSTICK_RIGHT = 0b0100000000000000;
const int16_t JOYSTICK_DOWN  = 0b0010000000000000;
const int16_t JOYSTICK_UP    = 0b0001000000000000;
const int16_t BUTTON_MASK    = 0b0000111111111111;

const uint8_t UTIL_P1_HOTKEY = 0b000001;
const uint8_t UTIL_P2_HOTKEY = 0b000010;
const uint8_t UTIL_P1_X1     = 0b000100;
const uint8_t UTIL_P1_X2     = 0b001000;
const uint8_t UTIL_P2_X1     = 0b010000;
const uint8_t UTIL_P2_X2     = 0b100000;

struct input_t {
    uint16_t p1;
    uint16_t p2;
    uint8_t util;
    int8_t p1_x;
    int8_t p1_y;
    int8_t p2_x;
    int8_t p2_y;
    bool changed;
};

bool operator==(const input_t& lhs, const input_t& rhs);
bool operator!=(const input_t& lhs, const input_t& rhs);

void picade_init();
input_t picade_get_input();

extern uint8_t input_debug[8];