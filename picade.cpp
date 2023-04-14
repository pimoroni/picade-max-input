#include "picade.hpp"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "picade.pio.h"

uint8_t picade_input_data[8] __attribute__((aligned(8))) = {0};
uint32_t transfer_count = 5 + 3;  // 5 bytes of input + 3 dummy bytes

bool operator==(const input_t& lhs, const input_t& rhs)
{
    return lhs.p1 == rhs.p1
        && lhs.p2 == rhs.p2
        && lhs.util == rhs.util
        && lhs.p1_x == rhs.p1_x
        && lhs.p1_y == rhs.p1_y
        && lhs.p2_x == rhs.p2_x
        && lhs.p2_y == rhs.p2_y;
}

void gpio_setup_input(uint pin) {
    gpio_init(pin);
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_set_dir(pin, false);
    gpio_set_pulls(pin, false, true);
}

void gpio_setup_output(PIO pio, uint pin) {
    pio_gpio_init(pio, pin);
    gpio_set_dir(pin, true);
    gpio_put(pin, 0);
}

void picade_init() {
    PIO pio = pio0;
    uint sm = 0;

    auto dma_control = dma_claim_unused_channel(true);
    auto dma_channel = dma_claim_unused_channel(true);

    // Input pins
    gpio_setup_input(5);
    gpio_setup_input(6);
    gpio_setup_input(7);
    gpio_setup_input(8);
    gpio_setup_input(9);
    gpio_setup_input(10);
    gpio_setup_input(11);
    gpio_setup_input(12);

    // Mux pins
    gpio_setup_output(pio, 0);
    gpio_setup_output(pio, 1);
    gpio_setup_output(pio, 2);
    gpio_setup_output(pio, 3);
    gpio_setup_output(pio, 4);

    pio_sm_claim(pio, sm);

    auto offset = pio_add_program(pio, &picade_scan_program);

    pio_sm_config config = picade_scan_program_get_default_config(offset);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);
    sm_config_set_in_pins(&config, 5);
    sm_config_set_sideset_pins(&config, 0);
    sm_config_set_sideset(&config, 5, false, false);
    auto div = clock_get_hz(clk_sys) / 30000;
    sm_config_set_clkdiv(&config, div);
    sm_config_set_in_shift(&config, false, true, 8);

    pio_sm_set_consecutive_pindirs(pio, sm, 5, 8, false);
    pio_sm_set_consecutive_pindirs(pio, sm, 0, 5, true);

    pio_sm_init(pio, sm, offset, &config);

    dma_channel_config dma_control_config = dma_channel_get_default_config(dma_control);
    channel_config_set_transfer_data_size(&dma_control_config, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_control_config, false);
    channel_config_set_write_increment(&dma_control_config, false);
    dma_channel_configure(dma_control,
        &dma_control_config,
        &dma_hw->ch[dma_channel].al1_transfer_count_trig,
        &transfer_count,
        1,
        false
    );

    dma_channel_config dma_config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_8);
    channel_config_set_bswap(&dma_config, false);
    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);
    channel_config_set_ring(&dma_config, true, 3); // Wrap at 8

    channel_config_set_dreq(&dma_config, pio_get_dreq(pio, sm, false));

    channel_config_set_chain_to(&dma_config, dma_control);

    dma_channel_configure(dma_channel,
        &dma_config,
        picade_input_data,
        &pio->rxf[sm],
        transfer_count,
        true
    );

    pio_sm_set_enabled(pio, sm, true);
}

// This serves to debounce the falling edge of buttons,
// particarly the joystick which can show contact bounce within the first 2ms
// A rising edge is always reported instantly, meaning latency is never affected by debounce
// however this short rolloff means- if you were some kind of superhuman or hooked your Picade to a signal generator-
// it cannot report button transitions faster than roughly debounce_depth milliseconds.
const uint debounce_depth = 3;  // How many reports- ostensibly milliseconds- before a low button should be reported as low
uint64_t debounce_fifo[debounce_depth][8] = {0};
uint debounce_fifo_idx = 0;

input_t picade_get_input() {
    static input_t last_in = {0, 0, 0, 0, 0, 0, 0, false};
    input_t in = {0, 0, 0, 0, 0, 0, 0, false};

    for(auto i = 0u; i < 8; i++) {
        debounce_fifo[debounce_fifo_idx][i] = picade_input_data[i];
    }
    debounce_fifo_idx++;
    debounce_fifo_idx %= debounce_depth;

    uint8_t input_data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    for(auto i = 0u; i < debounce_depth; i++) {
        for(auto j = 0u; j < 8; j++) {
            input_data[j] |= debounce_fifo[i][j];
        }
    }

    // Player 1, 12 buttons, 4 directions
    in.p1 = (input_data[0] & 0xf0) >> 4;    // 1, 2, 3, 4
    in.p1 |= (input_data[1] & 0xf0);        // 5, 6, 7, 8
    in.p1 |= (input_data[2] & 0xf0) << 4;   // 9, 10
    in.p1 |= (input_data[4] & 0xf0) << 8;   // joy

    // Player 2, 12 buttons, 4 directions
    in.p2 = (input_data[0] & 0x0f);
    in.p2 |= (input_data[1] & 0x0f) << 4;
    in.p2 |= (input_data[2] & 0x0f) << 8;
    in.p2 |= (input_data[4] & 0x0f) << 12;

    in.util = (input_data[3] & 0x07) | ((input_data[3] & 0x70) >> 1);

    if(in.p1 & JOYSTICK_LEFT) {in.p1_x = -127;}
    if(in.p1 & JOYSTICK_RIGHT){in.p1_x =  127;}
    if(in.p1 & JOYSTICK_UP)   {in.p1_y = -127;}
    if(in.p1 & JOYSTICK_DOWN) {in.p1_y =  127;}

    if(in.p2 & JOYSTICK_LEFT) {in.p2_x = -127;}
    if(in.p2 & JOYSTICK_RIGHT){in.p2_x =  127;}
    if(in.p2 & JOYSTICK_UP)   {in.p2_y = -127;}
    if(in.p2 & JOYSTICK_DOWN) {in.p2_y =  127;}

    in.changed = !(in == last_in);
    last_in = in;

    return in;
}