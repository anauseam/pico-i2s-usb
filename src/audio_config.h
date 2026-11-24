#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

// =============================================================================
// AUDIO CONFIGURATION
// =============================================================================
//
// Every setting of this firmware lives in this file: change the values below
// to match your board. A pin that can't do its job, or two settings that
// conflict, stop the build with a plain #error message (audio_config_check.h).
//
// Each setting sits in an #ifndef guard, so a build can preset it without
// editing this file, for example through CMake's AUDIO_CONFIG_DEFINES.
//
// README.md lists the allowed pins.
//
// =============================================================================

// --- Value names used by the settings below (don't edit) ---

#define PIN_NONE (-1) // the function isn't wired

#define I2S_ROLE_TARGET 1     // the ADC drives BCLK and LRCK
#define I2S_ROLE_CONTROLLER 2 // the Pico drives BCLK and LRCK (not implemented yet)

#define MCLK_SOURCE_EXTERNAL 1 // an oscillator drives the ADC's MCLK input
#define MCLK_SOURCE_PICO 2     // the Pico outputs 12.288 MHz (clk_sys / 5) on PIN_MCLK

#define CODEC_NONE 1     // a pin-strapped ADC, such as the PCM1808
#define CODEC_SGTL5000 2 // not implemented yet
#define CODEC_TAA5212 3  // not implemented yet

#define AUDIO_RATE_44100 (1 << 0)
#define AUDIO_RATE_48000 (1 << 1)
#define AUDIO_RATE_96000 (1 << 2)

// =============================================================================
// 1. PINS
// =============================================================================
// GPIO numbers. A control is present when its pin is set; PIN_NONE means the
// function isn't wired.

#ifndef PIN_I2S_DIN
#define PIN_I2S_DIN 2 // I2S data into the Pico (the ADC's DOUT)
#endif
#ifndef PIN_I2S_BCLK
#define PIN_I2S_BCLK 10 // I2S bit clock
#endif
#ifndef PIN_I2S_LRCK
#define PIN_I2S_LRCK 11 // I2S frame clock (also called FSYNC or WS)
#endif
#ifndef PIN_MCLK
#define PIN_MCLK 21 // MCLK output, used with MCLK_SOURCE_PICO
#endif
#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA PIN_NONE // codec control, used when CODEC isn't CODEC_NONE
#endif
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL PIN_NONE
#endif
#ifndef PIN_POT
#define PIN_POT PIN_NONE // volume pot wiper, an ADC pin (not implemented yet)
#endif
#ifndef PIN_MUTE_BUTTON
#define PIN_MUTE_BUTTON PIN_NONE // (not implemented yet)
#endif
#ifndef PIN_MUTE_LED
#define PIN_MUTE_LED PIN_NONE // (not implemented yet)
#endif
#ifndef PIN_MUTE_SWITCH
#define PIN_MUTE_SWITCH PIN_NONE // hardware mute input (not implemented yet)
#endif
#ifndef PIN_AVDD_EN
#define PIN_AVDD_EN PIN_NONE // analog supply enable (not implemented yet)
#endif

// =============================================================================
// 2. I2S ROLE AND MCLK
// =============================================================================

#ifndef I2S_ROLE
#define I2S_ROLE I2S_ROLE_TARGET // I2S_ROLE_TARGET or I2S_ROLE_CONTROLLER (not implemented yet)
#endif
#ifndef MCLK_SOURCE
#define MCLK_SOURCE MCLK_SOURCE_PICO // MCLK_SOURCE_PICO or MCLK_SOURCE_EXTERNAL
#endif

// =============================================================================
// 3. AUDIO FORMAT
// =============================================================================

#ifndef AUDIO_CHANNELS
#define AUDIO_CHANNELS 2 // 1 (not implemented yet) or 2
#endif

// The rates offered to the host, e.g. (AUDIO_RATE_48000 | AUDIO_RATE_96000).
// A pin-strapped ADC (CODEC_NONE) runs at exactly one rate.
#ifndef AUDIO_RATES
#define AUDIO_RATES (AUDIO_RATE_48000)
#endif

// Frames per DMA buffer (one sample per channel each).
#ifndef AUDIO_BUFFER_FRAMES
#define AUDIO_BUFFER_FRAMES 128
#endif

// =============================================================================
// 4. CODEC
// =============================================================================

#ifndef CODEC
#define CODEC CODEC_NONE // CODEC_NONE, CODEC_SGTL5000 or CODEC_TAA5212 (not implemented yet)
#endif

// =============================================================================
// 5. CONTROLS
// =============================================================================
// Used only when the control's pin is set.

#ifndef MUTE_BUTTON_ACTIVE_LOW
#define MUTE_BUTTON_ACTIVE_LOW 1 // 1: the input reads low while pressed
#endif
#ifndef MUTE_LED_ACTIVE_LOW
#define MUTE_LED_ACTIVE_LOW 0 // 1: the LED lights when the pin is driven low
#endif
#ifndef MUTE_SWITCH_ACTIVE_LOW
#define MUTE_SWITCH_ACTIVE_LOW 1 // 1: the input reads low while muted
#endif
// 1: the board's own circuit silences the audio; 0: the firmware mutes it.
#ifndef MUTE_SWITCH_CUTS_AUDIO
#define MUTE_SWITCH_CUTS_AUDIO 1
#endif

// =============================================================================
// 6. POWER
// =============================================================================

#ifndef USB_SUSPEND_POWER_DOWN
#define USB_SUSPEND_POWER_DOWN 0 // 1: power down while the host suspends USB (not implemented yet)
#endif

// =============================================================================
// 7. DEBUG
// =============================================================================

// Set to 1 to enable diagnostic UART printing in the main conductor loop.
// WARNING: High frequency printing can cause USB buffer starvation and audio glitches.
// Only enable for brief hardware verification.
#ifndef AUDIO_DEBUG_LOGGING
#define AUDIO_DEBUG_LOGGING 0
#endif

// Don't include the next two files directly, and keep them in this order: the
// checks use the derived values. (The blank line stops clang-format from
// sorting them.)
#include "audio_config_derived.h" // IWYU pragma: export

#include "audio_config_check.h" // IWYU pragma: export

#endif // AUDIO_CONFIG_H
