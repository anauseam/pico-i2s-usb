#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

// =============================================================================
// VALIDATED CLOCK CONFIGURATION
// =============================================================================
//
// Empirical testing has established that the Pico acting as I2S Controller
// (generating BCLK/LRCK) is never viable. Both Controller configurations fail:
//
//   CONTROLLER=1, MCLK=1 (Pico generates everything):
//     The PWM (MCLK) and PIO (BCLK/LRCK) use independent fractional dividers
//     of the 150 MHz PLL. These accumulate phase error relative to each other,
//     violating the PCM1808's requirement that BCLK/LRCK be coherently derived
//     from MCLK. Fails at all sample rates above 16 kHz.
//
//   CONTROLLER=1, MCLK=0 (Pico drives clocks, ADC drives MCLK):
//     The Pico's PLL and the external oscillator are physically independent
//     clock domains. LRCK will inevitably drift against MCLK. Guaranteed
//     hardware failure at all sample rates.
//
// The two VALID configurations (CONTROLLER=0) are:
//
//   CONTROLLER=0, MCLK=0  [DEFAULT - recommended for production]
//     External oscillator -> ADC (Master) -> Pico (Target).
//     All clocks trace to a single pristine source. Validated: 44.1/48/96 kHz.
//
//   CONTROLLER=0, MCLK=1  [Alternative - no external oscillator required]
//     Pico PWM -> ADC (Master) -> Pico (Target).
//     ADC derives coherent BCLK/LRCK internally from the jittery MCLK, which
//     it tolerates. Validated: 44.1/48/96 kHz.
//
// =============================================================================

// --- CONFIGURATION ---

// The Pico always acts as the I2S Target (Slave). The ADC is always the Master.
// DO NOT SET THIS TO 1. See note above.
#define USE_CONTROLLER_MODE 0

#if USE_CONTROLLER_MODE
#error "USE_CONTROLLER_MODE=1 is not a valid configuration. Both Controller " \
    "states (MCLK=0 and MCLK=1) have been empirically validated to fail. " \
    "Set USE_CONTROLLER_MODE to 0 and configure the ADC as I2S Master. " \
    "See audio_config.h for the full explanation."
#endif

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
#define SAMPLE_RATE 96000

// --- Double Buffer Setup ---
// 256 x 32-bit words = 128 stereo sample pairs per buffer
#define AUDIO_BUFFER_SIZE 256

#endif // AUDIO_CONFIG_H