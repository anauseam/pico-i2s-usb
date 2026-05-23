#ifndef USB_AUDIO_H
#define USB_AUDIO_H

#include <stdint.h>

// Initializes TinyUSB stack and audio interface
void usb_audio_init(void);

// TinyUSB device task, must be called in main loop
void usb_audio_task(void);

// Hands one full DMA buffer (n_words 32-bit samples, byte length must be a
// multiple of 4 by construction) to TinyUSB's ep_in_ff in a single write.
// Drops the entire buffer if ep_in_ff does not have room for the full write;
// partial writes are NEVER attempted (see R2.5 / R2.6).
void usb_audio_submit_buffer(uint32_t *buffer, uint32_t n_words);

// Returns the running count of DMA buffers dropped because ep_in_ff was full
// (or not yet open). Diagnostic only; used under AUDIO_DEBUG_LOGGING.
uint32_t usb_audio_get_overflow_count(void);

#endif // USB_AUDIO_H
