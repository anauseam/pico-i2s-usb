#include "i2s_audio.h"
#include "audio_config.h"
#include "hardware/clocks.h" // IWYU pragma: keep
#include <stdio.h>

// The Pico C/C++ SDK automatically generates these headers from your .pio files
#include "i2s_rx_target.pio.h"

#if MCLK_SOURCE == MCLK_SOURCE_PICO
static void setup_mclk_gpout(uint gpio) {
    // clk_sys / 5 = 12.288 MHz (256 × 48 kHz). With an odd divider the duty cycle is
    // 40 % unless DC50 is set. clock_gpio_init_int_frac16() rewrites CTRL and no SDK
    // call sets DC50, so it is set here afterwards, on this pin's GPOUT generator.
    clock_gpio_init_int_frac16(gpio, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, 5, 0);
    hw_set_bits(&clocks_hw->clk[gpio_to_gpout_clock_handle(gpio, clk_gpout0)].ctrl,
                CLOCKS_CLK_GPOUT0_CTRL_DC50_BITS);
}
#endif

void i2s_audio_init(PIO *pio_out, uint *sm_out) {
    // Defaulting to pio0 and state machine 0
    *pio_out = pio0;
    *sm_out = 0;
    uint offset;

#if MCLK_SOURCE == MCLK_SOURCE_PICO
    printf("Generating MCLK on GPIO %d: %lu Hz (clk_sys / 5 via CLK_GPOUT)\n", PIN_MCLK,
           (unsigned long)(clock_get_hz(clk_sys) / 5));
    setup_mclk_gpout(PIN_MCLK);
#endif

    printf("Starting I2S as TARGET (Expecting %d Hz from master)...\n", SAMPLE_RATE);

    offset = pio_add_program(*pio_out, &i2s_rx_target_program);
    i2s_rx_target_program_init(*pio_out, *sm_out, offset, PIN_I2S_DIN, PIN_I2S_BCLK, PIN_I2S_LRCK);
}

void i2s_audio_start(PIO pio, uint sm) {
    pio_sm_set_enabled(pio, sm, true);
}
