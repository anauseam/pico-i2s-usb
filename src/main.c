#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "hardware/uart.h" // IWYU pragma: keep
#include "pico/stdlib.h"   // IWYU pragma: keep
#include <stdio.h>         // IWYU pragma: keep
#include <string.h>        // IWYU pragma: keep

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
    char log_line[96];
    int log_len = 0;
    int log_pos = 0;
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
            if (++print_divider % 500 == 0 && log_pos == log_len) {
                snprintf(log_line, sizeof log_line,
                         "[Audio Data] L: 0x%08lX | R: 0x%08lX | overflow=%lu | overrun=%lu\r\n",
                         (unsigned long)ready_buffer[0], (unsigned long)ready_buffer[1],
                         (unsigned long)usb_audio_get_overflow_count(),
                         (unsigned long)dma_audio_get_overrun_count());
                log_len = (int)strlen(log_line);
                log_pos = 0;
            }
#endif
        }

#if AUDIO_DEBUG_LOGGING
        // printf would wait until the whole line has left the UART (stdio flushes after
        // every printf). Each pass hands over only what the UART's TX FIFO has room for.
        while (log_pos < log_len && uart_is_writable(uart_default)) {
            uart_putc_raw(uart_default, log_line[log_pos++]);
        }
#endif

        // Interrupts stay off from the check to the sleep: one that arrives in between
        // still ends the WFI. Every interrupt that leaves work for this loop belongs here.
        uint32_t irq = save_and_disable_interrupts();
        if (!usb_audio_event_pending() && !dma_audio_buffer_pending()) {
            __asm volatile("wfi");
        }
        restore_interrupts_from_disabled(irq);
    }

    return 0;
}