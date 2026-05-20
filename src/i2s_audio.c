#include "i2s_audio.h"
#include "audio_config.h"
#if GENERATE_MCLK
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#endif
#include <stdio.h>

// The Pico C/C++ SDK automatically generates these headers from your .pio files
#if USE_CONTROLLER_MODE
#include "i2s_rx_controller.pio.h"
#else
#include "i2s_rx_target.pio.h"
#endif

#if GENERATE_MCLK && (SAMPLE_RATE > 16000)
#warning                                                                                           \
    "PWM MCLK above 16kHz is empirically unreliable on RP2350 due to fractional divider jitter. Use an external clock source or custom board with 24.576MHz crystal for production use."
#endif

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

#if USE_CONTROLLER_MODE
    printf("Starting I2S as CONTROLLER (Targeting %d Hz)...\n", SAMPLE_RATE);
    offset = pio_add_program(*pio_out, &i2s_rx_controller_program);
    i2s_rx_controller_program_init(*pio_out, *sm_out, offset, PIN_DIN, PIN_CLOCK_BASE);

    float pio_freq = (float)SAMPLE_RATE * 64.0f * 2.0f;
    float clkdiv = (float)clock_get_hz(clk_sys) / pio_freq;
    pio_sm_set_clkdiv(*pio_out, *sm_out, clkdiv);
#else
    printf("Starting I2S as TARGET (Expecting %d Hz from master)...\n", SAMPLE_RATE);

    // ARCHITECTURAL LIMITATION: Currently, there is no documented recovery path or
    // watchdog for the PIO state machine if a BCLK/LRCK glitch causes frame misalignment.
    // If the external clock stutters, the SM may permanently offset the 32-bit frames.

    offset = pio_add_program(*pio_out, &i2s_rx_target_program);
    i2s_rx_target_program_init(*pio_out, *sm_out, offset, PIN_DIN, PIN_CLOCK_BASE);
#endif
}

void i2s_audio_start(PIO pio, uint sm) {
    pio_sm_set_enabled(pio, sm, true);
}