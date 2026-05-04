#include "usb_audio.h"
#include <stdio.h>

void usb_audio_init(void) {
    printf("TinyUSB Interface Initialized (Stubbed)\n");
    // TODO: Init TinyUSB (tud_init)
}

void usb_audio_send_buffer(uint32_t *buffer, uint32_t size) {
    // TODO: Write buffer to TinyUSB FIFO using the 32-bit container trick
}