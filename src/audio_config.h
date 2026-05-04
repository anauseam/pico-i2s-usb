#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

// --- CONFIGURATION ---

// Toggle this between 1 (Controller/Master) and 0 (Target/Slave)
#define USE_CONTROLLER_MODE 0

// Toggle MCLK generation independent of I2S role
#define GENERATE_MCLK 0

// Pin Setup
#define PIN_DIN 2         // Data In (from ADC DOUT)
#define PIN_CLOCK_BASE 10 // BCLK will be 10, LRCK will be 11
#define PIN_MCLK 12       // Master Clock out to ADC

// Audio Setup
#define SAMPLE_RATE 48000 // Supported: 44100, 48000, 96000

#if GENERATE_MCLK && (SAMPLE_RATE > 16000)
#warning                                                                                           \
    "PWM MCLK above 16kHz is empirically unreliable on RP2350 due to fractional divider jitter. Use an external clock source or custom board with 24.576MHz crystal for production use."
#endif

// --- DOUBLE BUFFER SETUP ---
#define AUDIO_BUFFER_SIZE 256 // Stores 128 Left and 128 Right 32-bit samples per buffer

#endif // AUDIO_CONFIG_H