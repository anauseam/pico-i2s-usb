#include "hardware/sync.h"
#include "pico/stdlib.h"
#include <stdio.h>

// Module Imports
#include "audio_config.h"
#include "dma_audio.h"
#include "i2s_audio.h"
#include "usb_audio.h"

int main() {
    stdio_init_all();
    sleep_ms(2000); // Give USB CDC time to enumerate

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
    int print_divider = 0;

    while (1) {
        // Poll the Buffer Manager
        if (dma_audio_get_ready_buffer(&ready_buffer)) {

            // Immediately hand off to the Host Interface!
            usb_audio_send_buffer(ready_buffer, AUDIO_BUFFER_SIZE);

            // Print occasionally for debug verification
            if (++print_divider % 100 == 0) {
                printf("[Audio Data] L: 0x%08X | R: 0x%08X\n", ready_buffer[0], ready_buffer[1]);
            }
        }

        // Put CPU to sleep to save power; wakes automatically on next DMA interrupt
        __asm volatile("wfi");
    }

    return 0;
}