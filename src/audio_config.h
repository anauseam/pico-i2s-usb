#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

// =============================================================================
// CLOCK CONFIGURATION
// =============================================================================
//
// The ADC is the I2S Controller and the Pico is the I2S Target (the
// Pico-as-Controller role is not implemented yet).
// Configure GENERATE_MCLK below depending on whether your hardware has an
// external oscillator feeding the ADC's MCLK pin (GENERATE_MCLK=0) or whether
// the Pico itself must generate MCLK via CLK_GPOUT (GENERATE_MCLK=1).
//
// See the "Clock Architecture" section of README.md and ARCHITECTURE.md §3.3.
//
// =============================================================================

// --- CONFIGURATION ---

// Set to 1 to enable diagnostic UART printing in the main conductor loop.
// WARNING: High frequency printing can cause USB buffer starvation and audio glitches.
// Only enable for brief hardware verification.
#define AUDIO_DEBUG_LOGGING 0

// Set to 1 to have the Pico output a 12.288 MHz MCLK (clk_sys / 5, CLK_GPOUT) on PIN_MCLK.
// Set to 0 if an external oscillator drives the ADC's MCLK input (default).
#define GENERATE_MCLK 0

// --- Pin Setup ---
#define PIN_DIN 2         // Data In (from ADC DOUT)
#define PIN_CLOCK_BASE 10 // BCLK on GPIO 10, LRCK on GPIO 11 (from ADC in Master mode)
// CLK_GPOUT pin: 13, 15 or 21 on the Pico 2 (only used if GENERATE_MCLK is 1).
#define PIN_MCLK 21

// --- Audio Setup ---
// Supported sample rates: 44100, 48000, 96000 with an external oscillator.
// With GENERATE_MCLK 1: the rates the ADC makes from 12.288 MHz (48000 at 256 fS).
#define SAMPLE_RATE 48000

// --- Double Buffer Setup ---
// 256 x 32-bit words = 128 stereo sample pairs per buffer
#define AUDIO_BUFFER_SIZE 256

#endif // AUDIO_CONFIG_H
