#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include <stdio.h>

// The Pico C/C++ SDK automatically generates these headers from your .pio files
#include "i2s_rx_controller.pio.h"
#include "i2s_rx_target.pio.h"

// --- CONFIGURATION ---

// Toggle this between 1 (Controller/Master) and 0 (Target/Slave)
#define USE_CONTROLLER_MODE 1

// Toggle MCLK generation independent of I2S role
#define GENERATE_MCLK 1

// Pin Setup
#define PIN_DIN 2         // Data In (from ADC DOUT)
#define PIN_CLOCK_BASE 10 // BCLK will be 10, LRCK will be 11
#define PIN_MCLK 12       // Master Clock out to ADC

// Audio Setup
#define SAMPLE_RATE 48000 // Supported: 44100, 48000, 96000

// DMA Buffer Setup
#define AUDIO_BUFFER_SIZE 256 // Stores 128 Left and 128 Right samples
uint32_t audio_buffer[AUDIO_BUFFER_SIZE];

// --- HELPER FUNCTIONS ---

#if USE_CONTROLLER_MODE || GENERATE_MCLK
// Configures the RP2350 PLL safely based on standard audio sample rates
void setup_audio_pll() {
#if SAMPLE_RATE == 96000
    // MCLK = 24.576 MHz. Multiplier = 6.
    // Sys Clock = 147.456 MHz
    set_sys_clock_khz(147456, true);
#elif SAMPLE_RATE == 48000
    // MCLK = 12.288 MHz. Multiplier = 12.
    // Sys Clock = 147.456 MHz
    set_sys_clock_khz(147456, true);
#elif SAMPLE_RATE == 44100
    // MCLK = 11.2896 MHz. Multiplier = 10.
    // Sys Clock = 112.896 MHz (Perfect integer kHz!)
    set_sys_clock_khz(112896, true);
#else
// Halt the compiler immediately if an unsupported rate is chosen!
#error "Unsupported SAMPLE_RATE! Please configure a valid rate (44100, 48000, or 96000)."
#endif
}
#endif

#if GENERATE_MCLK
// Configures a Hardware PWM slice to output the correct MCLK square wave
void setup_mclk_pwm(uint gpio) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.0f); // No fractional divider needed!

#if SAMPLE_RATE == 96000
    // 147.456 MHz / 6 = 24.576 MHz
    pwm_config_set_wrap(&config, 5); // 0..5 = 6 cycles
    pwm_set_gpio_level(gpio, 3);     // 50% duty
#elif SAMPLE_RATE == 48000
    // 147.456 MHz / 12 = 12.288 MHz
    pwm_config_set_wrap(&config, 11); // 0..11 = 12 cycles
    pwm_set_gpio_level(gpio, 6);      // 50% duty
#elif SAMPLE_RATE == 44100
    // 112.896 MHz / 10 = 11.2896 MHz
    pwm_config_set_wrap(&config, 9); // 0..9 = 10 cycles
    pwm_set_gpio_level(gpio, 5);     // 50% duty
#endif

    pwm_init(slice_num, &config, true);
}
#endif

// --- MAIN PROGRAM ---

int main() {
    // 1. If we are the controller or generating MCLK, configure the chip's brain for audio math
    // FIRST
#if USE_CONTROLLER_MODE || GENERATE_MCLK
    setup_audio_pll();
#endif

    // Now it is safe to initialize standard I/O (UART/USB)
    stdio_init_all();

    // Give USB CDC time to enumerate so we don't miss the setup printouts
    sleep_ms(2000);

    PIO pio = pio0;
    uint sm = 0;
    uint offset;

    // --- 2. CLOCK & PIO INITIALIZATION ---

#if GENERATE_MCLK
    printf("Generating MCLK on GPIO %d targeting %d Hz...\n", PIN_MCLK, SAMPLE_RATE);
    setup_mclk_pwm(PIN_MCLK);
#endif

#if USE_CONTROLLER_MODE
    printf("Starting I2S as CONTROLLER (Targeting %d Hz)...\n", SAMPLE_RATE);

    // Load the Controller program into PIO memory
    offset = pio_add_program(pio, &i2s_rx_controller_program);
    i2s_rx_controller_program_init(pio, sm, offset, PIN_DIN, PIN_CLOCK_BASE);

    // The controller MUST generate the BCLK.
    // Math: Sample Rate * 64 bits per frame * 2 PIO clock cycles per bit
    float pio_freq = (float)SAMPLE_RATE * 64.0f * 2.0f;
    float clkdiv = (float)clock_get_hz(clk_sys) / pio_freq;
    pio_sm_set_clkdiv(pio, sm, clkdiv);
#else
    printf("Starting I2S as TARGET (Expecting %d Hz from master)...\n", SAMPLE_RATE);
    offset = pio_add_program(pio, &i2s_rx_target_program);
    i2s_rx_target_program_init(pio, sm, offset, PIN_DIN, PIN_CLOCK_BASE);
#endif

    // --- 3. DMA INITIALIZATION (SINGLE BUFFER FOR TESTING) ---
    int dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_config = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);
    channel_config_set_dreq(&dma_config, pio_get_dreq(pio, sm, false));

    dma_channel_configure(dma_chan, &dma_config,
                          audio_buffer,      // Destination
                          &pio->rxf[sm],     // Source
                          AUDIO_BUFFER_SIZE, // Transfer count
                          true               // Start immediately
    );

    // --- 4. START THE PIO STATE MACHINE ---
    pio_sm_set_enabled(pio, sm, true);

    // --- 5. MAIN LOOP ---
    while (1) {
        // Wait for 1 full buffer of samples to be collected
        dma_channel_wait_for_finish_blocking(dma_chan);

        // Print the first L/R sample of the buffer to verify formatting
        printf("L: 0x%08X | R: 0x%08X\n", audio_buffer[0], audio_buffer[1]);

        // FIX: Must fully reconfigure to reset the transfer_count register
        dma_channel_configure(dma_chan, &dma_config,
                              audio_buffer,      // Destination
                              &pio->rxf[sm],     // Source
                              AUDIO_BUFFER_SIZE, // Reset Transfer count
                              true               // Trigger again!
        );
    }

    return 0;
}