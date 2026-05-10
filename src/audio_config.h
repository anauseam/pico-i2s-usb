#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

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
// Note: If GENERATE_MCLK is 1 and SAMPLE_RATE > 16000, you will see a build warning.
// PWM MCLK above 16kHz is empirically unreliable on RP2350 due to fractional divider jitter.
// For production use at 44.1/48/96kHz, it is highly recommended to use an external clock
// source (e.g. 24.576MHz crystal) instead of the internal PWM generator.
#define SAMPLE_RATE 48000 // Supported: 44100, 48000, 96000

// --- DOUBLE BUFFER SETUP ---
#define AUDIO_BUFFER_SIZE 256 // Stores 128 Left and 128 Right 32-bit samples per buffer

#endif // AUDIO_CONFIG_H