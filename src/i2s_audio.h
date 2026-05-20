#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include "hardware/pio.h"
#include "pico/stdlib.h" // IWYU pragma: keep

// Initializes the physical pins, clocks, and PIO state machines.
// Populates pio_out and sm_out with the selected PIO instance and State Machine.
void i2s_audio_init(PIO *pio_out, uint *sm_out);

// Starts the I2S hardware
void i2s_audio_start(PIO pio, uint sm);

#endif // I2S_AUDIO_H