#include "hardware/clocks.h"
#include "pico/stdlib.h" // IWYU pragma: keep
#include <stdio.h>

// Module Imports
#include "audio_config.h"
#include "dma_audio.h"
#include "i2s_audio.h"
#include "usb_audio.h"

int main() {
    // pll_sys: FOUTVCO = 12 MHz × FBDIV 128 = 1536 MHz; FOUTPOSTDIV = 1536 / 5 / 5
    // = 61.44 MHz, taken undivided as clk_sys. CLK_GPOUT divides clk_sys by 5 for the
    // 12.288 MHz MCLK. clk_sys must stay ≥ 1.1 × clk_usb (48 MHz) while USB runs
    // (erratum RP2350-E12).
    // Call before stdio_init_all(): this moves clk_peri, the UART's clock, to pll_usb.
    set_sys_clock_pll(1536000000, 5, 5);

    stdio_init_all();

    // --- 1. INITIALIZE HARDWARE SUBSYSTEMS ---
    PIO pio;
    uint sm;

    i2s_audio_init(&pio, &sm);
    dma_audio_init(pio, sm);
    usb_audio_init();

    // --- 2. START DATA PIPELINE ---
    // Start PIO first so clocks exist before DMA begins requesting data
    i2s_audio_start(pio, sm);
    dma_audio_start();

    // --- 3. THE CONDUCTOR LOOP ---
    uint32_t *ready_buffer = NULL;
#if AUDIO_DEBUG_LOGGING
    int print_divider = 0;
#endif

    while (1) {
        // Process TinyUSB device events
        usb_audio_task();

        // Poll the Buffer Manager
        if (dma_audio_get_ready_buffer(&ready_buffer)) {

            // Immediately hand off to the Host Interface (writes directly into
            // TinyUSB's ep_in_ff).
            // See docs/internals/02-audio-pipeline.md
            usb_audio_submit_buffer(ready_buffer, AUDIO_BUFFER_SIZE);

#if AUDIO_DEBUG_LOGGING
            // Print occasionally for debug verification.
            // 500 buffers * ~2.66ms = ~1.3 seconds per print
            if (++print_divider % 500 == 0) {
                printf("[Audio Data] L: 0x%08X | R: 0x%08X | overflow=%u\n", ready_buffer[0],
                       ready_buffer[1], (unsigned)usb_audio_get_overflow_count());
            }
#endif
        }

        // Put CPU to sleep to save power; wakes automatically on next DMA interrupt
        // or USB interrupt.
        __asm volatile("wfi");
    }

    return 0;
}