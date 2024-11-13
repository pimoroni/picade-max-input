#include "plasma.hpp"
#include "hardware/spi.h"
#include "hardware/dma.h"

// TODO I count 30 inputs on the board- 12 per player + 6 util so we're probably OK with 32 buttons * 4 LEDs * 4 bytes?
uint8_t led_buffer[32 * 4 * 4] = {0};
uint8_t led_front_buffer[32 * 4 * 4] = {0};

// TODO these might need dialling in but seem okay on my 4x4 rig
uint8_t apa102_sof[8] = {0x00};
uint8_t apa102_eof[8] = {0xff};

// Say no to magic numbers
const uint8_t APA102_SOF = 0b11100000;

uint spi_channel = 0;

void dma_handler() {
    if(dma_irqn_get_channel_status(0, spi_channel)){
        dma_irqn_acknowledge_channel(0, spi_channel);
        spi_write_blocking(spi0, apa102_eof, sizeof(apa102_eof));
        spi_write_blocking(spi0, apa102_sof, sizeof(apa102_sof));
        dma_channel_set_read_addr(spi_channel, led_buffer, true);
    }
}

void plasma_init() {
    plasma_set_all(0, 0, 0);

    spi_init(spi0, 2 * 1000 * 1000);
    gpio_set_function(PLASMA_CLOCK, GPIO_FUNC_SPI);
    gpio_set_function(PLASMA_DATA, GPIO_FUNC_SPI);

    spi_write_blocking(spi0, apa102_sof, sizeof(apa102_sof));

    spi_channel = dma_claim_unused_channel(true);
    dma_channel_config spi_config = dma_channel_get_default_config(spi_channel);
    channel_config_set_transfer_data_size(&spi_config, DMA_SIZE_8);
    channel_config_set_dreq(&spi_config, spi_get_dreq(spi0, true));
    channel_config_set_write_increment(&spi_config, false);
    //channel_config_set_ring(&spi_config, false, 9); // Wrap at 512 bytes

    dma_channel_set_irq0_enabled(spi_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_configure(spi_channel, &spi_config,
                          &spi_get_hw(spi0)->dr,
                          led_buffer,
                          sizeof(led_buffer),
                          true);
}

void plasma_flip() {
    /*
    Plasma is     SOF B G R
    Multiverse is B G R _
    */
    for(auto x = 0u; x < sizeof(led_buffer); x += 4) {
        led_buffer[x + 0] = APA102_SOF | led_front_buffer[x + 3];
        led_buffer[x + 1] = led_front_buffer[x + 0];
        led_buffer[x + 2] = led_front_buffer[x + 1];
        led_buffer[x + 3] = led_front_buffer[x + 2];
    }
}

void plasma_set_all(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    for(auto x = 0u; x < sizeof(led_buffer); x += 4) {
        led_buffer[x + 0] = APA102_SOF | brightness;
        led_buffer[x + 1] = b;
        led_buffer[x + 2] = g;
        led_buffer[x + 3] = r;
    }
}
