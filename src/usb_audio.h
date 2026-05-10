#ifndef USB_AUDIO_H
#define USB_AUDIO_H

#include <stdint.h>

// Initializes TinyUSB stack and audio interface
void usb_audio_init(void);

// TinyUSB device task, must be called in main loop
void usb_audio_task(void);

// Accepts a pointer to a fully packed 32-bit audio buffer and sends it over USB
void usb_audio_send_buffer(uint32_t *buffer, uint32_t size);

#endif // USB_AUDIO_H