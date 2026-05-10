#include "pico/stdlib.h"
#include <stdio.h>

// Module Imports
#include "usb_audio.h"

int main() {
    stdio_init_all();

    // --- USB-ONLY TEST ---
    // All PIO/DMA disabled to isolate USB stack behavior
    usb_audio_init();

    uint32_t last_led_time = to_ms_since_boot(get_absolute_time());
    bool led_state = false;

    while (1) {
        // Process TinyUSB device events
        usb_audio_task();

        // Send dummy audio data to keep the Isochronous endpoint happy
        static uint32_t dummy_audio[128] = {0};
        usb_audio_send_buffer(dummy_audio, 128);

        // Heartbeat LED - should blink forever if no crash
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        if (current_time - last_led_time > 100) {
            last_led_time = current_time;
            led_state = !led_state;
#ifdef PICO_DEFAULT_LED_PIN
            gpio_put(PICO_DEFAULT_LED_PIN, led_state);
#endif
        }
    }

    return 0;
}