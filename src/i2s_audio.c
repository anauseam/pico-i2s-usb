#include "i2s_audio.h"
#include "audio_config.h"
#if GENERATE_MCLK
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#endif
#include <stdio.h>

// The Pico C/C++ SDK automatically generates these headers from your .pio files
#include "i2s_rx_target.pio.h"

#if GENERATE_MCLK
static void setup_mclk_pwm(uint gpio) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_config config = pwm_get_default_config();

    uint32_t mclk_freq = SAMPLE_RATE * 256;
    // With wrap=1, the PWM period is 2 clock ticks (tick 0 and tick 1).
    // Therefore, the effective frequency is sys_clk / (divider * 2).
    // This is why we multiply the target frequency by 2 here.
    float divider = (float)clock_get_hz(clk_sys) / (float)(mclk_freq * 2);

    pwm_config_set_clkdiv(&config, divider);
    pwm_config_set_wrap(&config, 1);

    pwm_init(slice_num, &config, true); // Init first, then set level, else it will start at 0
    pwm_set_gpio_level(gpio, 1);        // 50% duty cycle
}
#endif

void i2s_audio_init(PIO *pio_out, uint *sm_out) {
    // Defaulting to pio0 and state machine 0
    *pio_out = pio0;
    *sm_out = 0;
    uint offset;

#if GENERATE_MCLK
    printf("Generating MCLK on GPIO %d targeting %d Hz...\n", PIN_MCLK, SAMPLE_RATE);
    setup_mclk_pwm(PIN_MCLK);
#endif

    printf("Starting I2S as TARGET (Expecting %d Hz from master)...\n", SAMPLE_RATE);

    // ARCHITECTURAL LIMITATION: Currently, there is no documented recovery path or
    // watchdog for the PIO state machine if a BCLK/LRCK glitch causes frame misalignment.
    // If the external clock stutters, the SM may permanently offset the 32-bit frames.
    // Background: see docs/internals/suspected-issues.md#pio-frame-misalignment-recovery

    offset = pio_add_program(*pio_out, &i2s_rx_target_program);
    i2s_rx_target_program_init(*pio_out, *sm_out, offset, PIN_DIN, PIN_CLOCK_BASE);
}

void i2s_audio_start(PIO pio, uint sm) {
    pio_sm_set_enabled(pio, sm, true);
}
