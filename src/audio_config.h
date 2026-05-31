#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

// =============================================================================
// VALIDATED CLOCK CONFIGURATION
// =============================================================================
//
// The Pico is ALWAYS the I2S Target. The ADC is ALWAYS the I2S Controller.
// Configure GENERATE_MCLK below depending on whether your hardware has an
// external oscillator feeding the ADC's MCLK pin (GENERATE_MCLK=0) or whether
// the Pico itself must generate MCLK via PWM (GENERATE_MCLK=1).
//
// For the full historical rationale (why Pico-as-Controller is infeasible
// and which configurations were empirically tested), see ARCHITECTURE.md and
// the "Clock Architecture" section of README.md.
//
// =============================================================================

// --- CONFIGURATION ---

// Set to 1 to enable diagnostic UART printing in the main conductor loop.
// WARNING: High frequency printing can cause USB buffer starvation and audio glitches.
// Only enable for brief hardware verification.
#define AUDIO_DEBUG_LOGGING 0

// Set to 1 if the Pico should generate MCLK via PWM (no external oscillator).
// Set to 0 if you have an external oscillator driving the ADC directly (recommended).
#define GENERATE_MCLK 0

// --- Pin Setup ---
#define PIN_DIN 2         // Data In (from ADC DOUT)
#define PIN_CLOCK_BASE 10 // BCLK on GPIO 10, LRCK on GPIO 11 (from ADC in Master mode)
#define PIN_MCLK 12       // Master Clock out to ADC (only used if GENERATE_MCLK is 1)

// --- Audio Setup ---
// Supported sample rates: 44100, 48000, 96000
#define SAMPLE_RATE 48000

// --- Double Buffer Setup ---
// 256 x 32-bit words = 128 stereo sample pairs per buffer
#define AUDIO_BUFFER_SIZE 256

#endif // AUDIO_CONFIG_H
