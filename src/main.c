#include "pico/stdlib.h" // IWYU pragma: keep
#include <stdio.h>

// Module Imports
#include "audio_config.h"
#include "dma_audio.h"
#include "i2s_audio.h"
#include "usb_audio.h"

int main() {
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

            // Immediately hand off to the Host Interface!
            usb_audio_send_buffer(ready_buffer, AUDIO_BUFFER_SIZE);

#if AUDIO_DEBUG_LOGGING
            // Print occasionally for debug verification.
            // 500 buffers * ~2.66ms = ~1.3 seconds per print
            if (++print_divider % 500 == 0) {
                printf("[Audio Data] L: 0x%08X | R: 0x%08X\n", ready_buffer[0], ready_buffer[1]);
            }
#endif
        }

        // Put CPU to sleep to save power; wakes automatically on next DMA interrupt
        // or USB interrupt.
        __asm volatile("wfi");
    }

    return 0;
}