#ifndef AUDIO_CONFIG_CHECK_H
#define AUDIO_CONFIG_CHECK_H

// Checks of the settings in audio_config.h. A failed check stops the build with
// a plain #error that names the setting to change. GCC reports every failed
// check; the first message is the cause. Don't include this file directly;
// include audio_config.h.
//
// A pin is checked only while it is in use:
//   PIN_I2S_DIN, PIN_I2S_BCLK, PIN_I2S_LRCK   always
//   PIN_MCLK                                  MCLK_SOURCE is MCLK_SOURCE_PICO
//   PIN_I2C_SDA, PIN_I2C_SCL                  CODEC isn't CODEC_NONE
//   every other pin                           when set (not PIN_NONE)
//
// Keep the start of each #error message, up to the colon, on its first line:
// scripts/build-matrix.sh matches that part.
//
// Page numbers refer to the RP2350 datasheet.

#include "pico.h" // IWYU pragma: keep (chip and board facts: PICO_RP2350A, board and console pins)

// =============================================================================
// Helpers
// =============================================================================

#define CFG_USES_MCLK (MCLK_SOURCE == MCLK_SOURCE_PICO)
#define CFG_USES_I2C (CODEC != CODEC_NONE)

// GPIOs of this chip: 30 on QFN-60 chips (PICO_RP2350A is 1), 48 on QFN-80
// chips. Not the SDK's NUM_BANK0_GPIOS: it is unsigned, and comparing PIN_NONE
// with it makes GCC warn even where the comparison isn't evaluated.
#if PICO_RP2350A
#define CFG_NUM_GPIOS 30
#else
#define CFG_NUM_GPIOS 48
#endif

#define CFG_PIN_VALID(pin) ((pin) >= 0 && (pin) < CFG_NUM_GPIOS)
#define CFG_PIN_SET(pin) ((pin) != PIN_NONE)
#define CFG_IS_FLAG(value) ((value) == 0 || (value) == 1)

// "& 63" keeps the shift defined for out-of-range pins, which a check below reports.
#define CFG_PIN_BIT(pin) ((pin) == PIN_NONE ? 0 : (1ULL << ((pin) & 63)))

#define CFG_BIT_I2S_DIN CFG_PIN_BIT(PIN_I2S_DIN)
#define CFG_BIT_I2S_BCLK CFG_PIN_BIT(PIN_I2S_BCLK)
#define CFG_BIT_I2S_LRCK CFG_PIN_BIT(PIN_I2S_LRCK)
#define CFG_BIT_MCLK (CFG_USES_MCLK ? CFG_PIN_BIT(PIN_MCLK) : 0)
#define CFG_BIT_I2C_SDA (CFG_USES_I2C ? CFG_PIN_BIT(PIN_I2C_SDA) : 0)
#define CFG_BIT_I2C_SCL (CFG_USES_I2C ? CFG_PIN_BIT(PIN_I2C_SCL) : 0)
#define CFG_BIT_POT CFG_PIN_BIT(PIN_POT)
#define CFG_BIT_MUTE_BUTTON CFG_PIN_BIT(PIN_MUTE_BUTTON)
#define CFG_BIT_MUTE_LED CFG_PIN_BIT(PIN_MUTE_LED)
#define CFG_BIT_MUTE_SWITCH CFG_PIN_BIT(PIN_MUTE_SWITCH)
#define CFG_BIT_AVDD_EN CFG_PIN_BIT(PIN_AVDD_EN)

// =============================================================================
// Every setting holds a known value
// =============================================================================
// The value names start at 1, so a misspelt name, which #if reads as 0, fails.

#if I2S_ROLE != I2S_ROLE_TARGET && I2S_ROLE != I2S_ROLE_CONTROLLER
#error "I2S_ROLE must be I2S_ROLE_TARGET or I2S_ROLE_CONTROLLER"
#endif
#if MCLK_SOURCE != MCLK_SOURCE_EXTERNAL && MCLK_SOURCE != MCLK_SOURCE_PICO
#error "MCLK_SOURCE must be MCLK_SOURCE_EXTERNAL or MCLK_SOURCE_PICO"
#endif
#if CODEC != CODEC_NONE && CODEC != CODEC_SGTL5000 && CODEC != CODEC_TAA5212
#error "CODEC must be CODEC_NONE, CODEC_SGTL5000 or CODEC_TAA5212"
#endif

#define CFG_ALL_RATES (AUDIO_RATE_44100 | AUDIO_RATE_48000 | AUDIO_RATE_96000)
#if (AUDIO_RATES) == 0 || ((AUDIO_RATES) & ~CFG_ALL_RATES)
#error "AUDIO_RATES must list at least one AUDIO_RATE_* value, and nothing else"
#endif

#if AUDIO_CHANNELS != 1 && AUDIO_CHANNELS != 2
#error "AUDIO_CHANNELS must be 1 or 2"
#endif
#if AUDIO_BUFFER_FRAMES < 1
#error "AUDIO_BUFFER_FRAMES must be at least 1"
#endif
#if !CFG_IS_FLAG(MUTE_BUTTON_ACTIVE_LOW)
#error "MUTE_BUTTON_ACTIVE_LOW must be 0 or 1"
#endif
#if !CFG_IS_FLAG(MUTE_LED_ACTIVE_LOW)
#error "MUTE_LED_ACTIVE_LOW must be 0 or 1"
#endif
#if !CFG_IS_FLAG(MUTE_SWITCH_ACTIVE_LOW)
#error "MUTE_SWITCH_ACTIVE_LOW must be 0 or 1"
#endif
#if !CFG_IS_FLAG(MUTE_SWITCH_CUTS_AUDIO)
#error "MUTE_SWITCH_CUTS_AUDIO must be 0 or 1"
#endif
#if !CFG_IS_FLAG(USB_SUSPEND_POWER_DOWN)
#error "USB_SUSPEND_POWER_DOWN must be 0 or 1"
#endif
#if !CFG_IS_FLAG(AUDIO_DEBUG_LOGGING)
#error "AUDIO_DEBUG_LOGGING must be 0 or 1"
#endif

// =============================================================================
// Each in-use pin can do its job
// =============================================================================

// The I2S pins stay within GPIO 0-31, the PIO's pin window. This only matters on
// QFN-80 chips.
#if !CFG_PIN_VALID(PIN_I2S_DIN) || PIN_I2S_DIN > 31
#error "PIN_I2S_DIN must be a GPIO number of this chip (and at most 31)"
#endif
#if !CFG_PIN_VALID(PIN_I2S_BCLK) || PIN_I2S_BCLK > 31
#error "PIN_I2S_BCLK must be a GPIO number of this chip (and at most 31)"
#endif
#if !CFG_PIN_VALID(PIN_I2S_LRCK) || PIN_I2S_LRCK > 31
#error "PIN_I2S_LRCK must be a GPIO number of this chip (and at most 31)"
#endif

// The other pins, while in use and set. A check below reports a pin in use but not set.
#if CFG_USES_MCLK && CFG_PIN_SET(PIN_MCLK) && !CFG_PIN_VALID(PIN_MCLK)
#error "PIN_MCLK must be a GPIO number of this chip"
#endif
#if CFG_USES_I2C && CFG_PIN_SET(PIN_I2C_SDA) && !CFG_PIN_VALID(PIN_I2C_SDA)
#error "PIN_I2C_SDA must be a GPIO number of this chip"
#endif
#if CFG_USES_I2C && CFG_PIN_SET(PIN_I2C_SCL) && !CFG_PIN_VALID(PIN_I2C_SCL)
#error "PIN_I2C_SCL must be a GPIO number of this chip"
#endif
#if CFG_PIN_SET(PIN_POT) && !CFG_PIN_VALID(PIN_POT)
#error "PIN_POT must be a GPIO number of this chip"
#endif
#if CFG_PIN_SET(PIN_MUTE_BUTTON) && !CFG_PIN_VALID(PIN_MUTE_BUTTON)
#error "PIN_MUTE_BUTTON must be a GPIO number of this chip"
#endif
#if CFG_PIN_SET(PIN_MUTE_LED) && !CFG_PIN_VALID(PIN_MUTE_LED)
#error "PIN_MUTE_LED must be a GPIO number of this chip"
#endif
#if CFG_PIN_SET(PIN_MUTE_SWITCH) && !CFG_PIN_VALID(PIN_MUTE_SWITCH)
#error "PIN_MUTE_SWITCH must be a GPIO number of this chip"
#endif
#if CFG_PIN_SET(PIN_AVDD_EN) && !CFG_PIN_VALID(PIN_AVDD_EN)
#error "PIN_AVDD_EN must be a GPIO number of this chip"
#endif

// Clock-output pins: pp. 18-19.
#if CFG_USES_MCLK && CFG_PIN_SET(PIN_MCLK) && PIN_MCLK != 13 && PIN_MCLK != 15 &&                  \
    PIN_MCLK != 21 && PIN_MCLK != 23 && PIN_MCLK != 24 && PIN_MCLK != 25
#error "PIN_MCLK must be a clock-output pin: GPIO 13, 15, 21, 23, 24 or 25"
#endif

// SDA pins are GPIO 0, 4, 8, ... (block I2C0) and 2, 6, 10, ... (block I2C1). Each
// block's SCL pins are its SDA pins + 1, and SCL may be any SCL pin of SDA's block
// (pp. 18, 589).
#if CFG_USES_I2C && CFG_PIN_SET(PIN_I2C_SDA) && PIN_I2C_SDA % 4 != 0 && PIN_I2C_SDA % 4 != 2
#error "PIN_I2C_SDA must be an I2C SDA pin (GPIO number % 4 is 0 or 2)"
#endif
#if CFG_USES_I2C && CFG_PIN_SET(PIN_I2C_SDA) && CFG_PIN_SET(PIN_I2C_SCL) &&                        \
    PIN_I2C_SCL % 4 != PIN_I2C_SDA % 4 + 1
#error "PIN_I2C_SCL must be an SCL pin of the same I2C block as PIN_I2C_SDA"
#endif

// The package decides the ADC pins: GPIO 26-29 on QFN-60 chips (PICO_RP2350A is 1),
// GPIO 40-47 on QFN-80 chips (p. 1067).
#if PICO_RP2350A && CFG_PIN_SET(PIN_POT) && (PIN_POT < 26 || PIN_POT > 29)
#error "PIN_POT must be an ADC pin: GPIO 26 to 29"
#endif
#if !PICO_RP2350A && CFG_PIN_SET(PIN_POT) && (PIN_POT < 40 || PIN_POT > 47)
#error "PIN_POT must be an ADC pin: GPIO 40 to 47"
#endif

// =============================================================================
// No pin clashes with the board, the debug console or another pin
// =============================================================================

// The board header that PICO_BOARD selects names the GPIOs the board itself uses
// (pico2.h: SMPS mode 23, VBUS sense 24, VSYS sense 29, LED 25); a bare-chip board
// header names none. The board's LED is allowed for PIN_MUTE_LED.
#ifdef PICO_SMPS_MODE_PIN
#define CFG_BOARD_SMPS_BIT CFG_PIN_BIT(PICO_SMPS_MODE_PIN)
#else
#define CFG_BOARD_SMPS_BIT 0
#endif
#ifdef PICO_VBUS_PIN
#define CFG_BOARD_VBUS_BIT CFG_PIN_BIT(PICO_VBUS_PIN)
#else
#define CFG_BOARD_VBUS_BIT 0
#endif
#ifdef PICO_VSYS_PIN
#define CFG_BOARD_VSYS_BIT CFG_PIN_BIT(PICO_VSYS_PIN)
#else
#define CFG_BOARD_VSYS_BIT 0
#endif
#ifdef PICO_DEFAULT_LED_PIN
#define CFG_BOARD_LED_BIT CFG_PIN_BIT(PICO_DEFAULT_LED_PIN)
#else
#define CFG_BOARD_LED_BIT 0
#endif
#define CFG_BOARD_MASK (CFG_BOARD_SMPS_BIT | CFG_BOARD_VBUS_BIT | CFG_BOARD_VSYS_BIT)
#define CFG_BOARD_MASK_WITH_LED (CFG_BOARD_MASK | CFG_BOARD_LED_BIT)

#if CFG_BIT_I2S_DIN & CFG_BOARD_MASK_WITH_LED
#error "PIN_I2S_DIN is on a GPIO the board itself uses (see the board header)"
#endif
#if CFG_BIT_I2S_BCLK & CFG_BOARD_MASK_WITH_LED
#error "PIN_I2S_BCLK is on a GPIO the board itself uses (see the board header)"
#endif
#if CFG_BIT_I2S_LRCK & CFG_BOARD_MASK_WITH_LED
#error "PIN_I2S_LRCK is on a GPIO the board itself uses (see the board header)"
#endif
#if CFG_BIT_MCLK & CFG_BOARD_MASK_WITH_LED
#error "PIN_MCLK is on a GPIO the board itself uses (see the board header)"
#endif
#if CFG_BIT_I2C_SDA & CFG_BOARD_MASK_WITH_LED
#error "PIN_I2C_SDA is on a GPIO the board itself uses (see the board header)"
#endif
#if CFG_BIT_I2C_SCL & CFG_BOARD_MASK_WITH_LED
#error "PIN_I2C_SCL is on a GPIO the board itself uses (see the board header)"
#endif
#if CFG_BIT_POT & CFG_BOARD_MASK_WITH_LED
#error "PIN_POT is on a GPIO the board itself uses (see the board header)"
#endif
#if CFG_BIT_MUTE_BUTTON & CFG_BOARD_MASK_WITH_LED
#error "PIN_MUTE_BUTTON is on a GPIO the board itself uses (see the board header)"
#endif
#if CFG_BIT_MUTE_LED & CFG_BOARD_MASK
#error "PIN_MUTE_LED is on a GPIO the board itself uses (see the board header)"
#endif
#if CFG_BIT_MUTE_SWITCH & CFG_BOARD_MASK_WITH_LED
#error "PIN_MUTE_SWITCH is on a GPIO the board itself uses (see the board header)"
#endif
#if CFG_BIT_AVDD_EN & CFG_BOARD_MASK_WITH_LED
#error "PIN_AVDD_EN is on a GPIO the board itself uses (see the board header)"
#endif

// The debug console's pins count while the console is on (pico_enable_stdio_uart in
// CMakeLists.txt). They come from the board header unless the build moves them with
// PICO_DEFAULT_UART_TX_PIN / _RX_PIN.
#if defined(LIB_PICO_STDIO_UART) && LIB_PICO_STDIO_UART && defined(PICO_DEFAULT_UART_TX_PIN)
#define CFG_CONSOLE_TX_BIT CFG_PIN_BIT(PICO_DEFAULT_UART_TX_PIN)
#else
#define CFG_CONSOLE_TX_BIT 0
#endif
#if defined(LIB_PICO_STDIO_UART) && LIB_PICO_STDIO_UART && defined(PICO_DEFAULT_UART_RX_PIN)
#define CFG_CONSOLE_RX_BIT CFG_PIN_BIT(PICO_DEFAULT_UART_RX_PIN)
#else
#define CFG_CONSOLE_RX_BIT 0
#endif
#define CFG_CONSOLE_MASK (CFG_CONSOLE_TX_BIT | CFG_CONSOLE_RX_BIT)

#if CFG_BIT_I2S_DIN & CFG_CONSOLE_MASK
#error "PIN_I2S_DIN is on the debug console's UART pins"
#endif
#if CFG_BIT_I2S_BCLK & CFG_CONSOLE_MASK
#error "PIN_I2S_BCLK is on the debug console's UART pins"
#endif
#if CFG_BIT_I2S_LRCK & CFG_CONSOLE_MASK
#error "PIN_I2S_LRCK is on the debug console's UART pins"
#endif
#if CFG_BIT_MCLK & CFG_CONSOLE_MASK
#error "PIN_MCLK is on the debug console's UART pins"
#endif
#if CFG_BIT_I2C_SDA & CFG_CONSOLE_MASK
#error "PIN_I2C_SDA is on the debug console's UART pins"
#endif
#if CFG_BIT_I2C_SCL & CFG_CONSOLE_MASK
#error "PIN_I2C_SCL is on the debug console's UART pins"
#endif
#if CFG_BIT_POT & CFG_CONSOLE_MASK
#error "PIN_POT is on the debug console's UART pins"
#endif
#if CFG_BIT_MUTE_BUTTON & CFG_CONSOLE_MASK
#error "PIN_MUTE_BUTTON is on the debug console's UART pins"
#endif
#if CFG_BIT_MUTE_LED & CFG_CONSOLE_MASK
#error "PIN_MUTE_LED is on the debug console's UART pins"
#endif
#if CFG_BIT_MUTE_SWITCH & CFG_CONSOLE_MASK
#error "PIN_MUTE_SWITCH is on the debug console's UART pins"
#endif
#if CFG_BIT_AVDD_EN & CFG_CONSOLE_MASK
#error "PIN_AVDD_EN is on the debug console's UART pins"
#endif

// Each pin is compared with the pins listed before it, so the message names the
// second one.
#define CFG_USED_1 CFG_BIT_I2S_DIN
#if CFG_BIT_I2S_BCLK & CFG_USED_1
#error "PIN_I2S_BCLK uses a GPIO that an earlier pin setting already uses"
#endif
#define CFG_USED_2 (CFG_USED_1 | CFG_BIT_I2S_BCLK)
#if CFG_BIT_I2S_LRCK & CFG_USED_2
#error "PIN_I2S_LRCK uses a GPIO that an earlier pin setting already uses"
#endif
#define CFG_USED_3 (CFG_USED_2 | CFG_BIT_I2S_LRCK)
#if CFG_BIT_MCLK & CFG_USED_3
#error "PIN_MCLK uses a GPIO that an earlier pin setting already uses"
#endif
#define CFG_USED_4 (CFG_USED_3 | CFG_BIT_MCLK)
#if CFG_BIT_I2C_SDA & CFG_USED_4
#error "PIN_I2C_SDA uses a GPIO that an earlier pin setting already uses"
#endif
#define CFG_USED_5 (CFG_USED_4 | CFG_BIT_I2C_SDA)
#if CFG_BIT_I2C_SCL & CFG_USED_5
#error "PIN_I2C_SCL uses a GPIO that an earlier pin setting already uses"
#endif
#define CFG_USED_6 (CFG_USED_5 | CFG_BIT_I2C_SCL)
#if CFG_BIT_POT & CFG_USED_6
#error "PIN_POT uses a GPIO that an earlier pin setting already uses"
#endif
#define CFG_USED_7 (CFG_USED_6 | CFG_BIT_POT)
#if CFG_BIT_MUTE_BUTTON & CFG_USED_7
#error "PIN_MUTE_BUTTON uses a GPIO that an earlier pin setting already uses"
#endif
#define CFG_USED_8 (CFG_USED_7 | CFG_BIT_MUTE_BUTTON)
#if CFG_BIT_MUTE_LED & CFG_USED_8
#error "PIN_MUTE_LED uses a GPIO that an earlier pin setting already uses"
#endif
#define CFG_USED_9 (CFG_USED_8 | CFG_BIT_MUTE_LED)
#if CFG_BIT_MUTE_SWITCH & CFG_USED_9
#error "PIN_MUTE_SWITCH uses a GPIO that an earlier pin setting already uses"
#endif
#define CFG_USED_10 (CFG_USED_9 | CFG_BIT_MUTE_SWITCH)
#if CFG_BIT_AVDD_EN & CFG_USED_10
#error "PIN_AVDD_EN uses a GPIO that an earlier pin setting already uses"
#endif

// =============================================================================
// Settings that conflict
// =============================================================================
// The USB FIFO check is in tusb_config.h, which defines the FIFO size.

#if CODEC == CODEC_NONE && AUDIO_RATE_COUNT != 1
#error "CODEC_NONE (a pin-strapped ADC) runs at one rate: AUDIO_RATES must list exactly one"
#endif

#if CFG_USES_MCLK && ((AUDIO_RATES) & AUDIO_RATE_44100)
#error "AUDIO_RATE_44100 needs MCLK_SOURCE_EXTERNAL: 11.2896 MHz can't be " \
       "divided from the 12 MHz crystal"
#endif

#if I2S_ROLE == I2S_ROLE_CONTROLLER && MCLK_SOURCE == MCLK_SOURCE_EXTERNAL
#error "With I2S_ROLE_CONTROLLER, MCLK_SOURCE can't be MCLK_SOURCE_EXTERNAL: an external " \
       "oscillator and the Pico's crystal are asynchronous, and the ADC needs MCLK and " \
       "LRCK synchronous"
#endif

#if CFG_USES_MCLK && !CFG_PIN_SET(PIN_MCLK)
#error "MCLK_SOURCE_PICO needs PIN_MCLK"
#endif
#if CFG_USES_I2C && (!CFG_PIN_SET(PIN_I2C_SDA) || !CFG_PIN_SET(PIN_I2C_SCL))
#error "CODEC needs PIN_I2C_SDA and PIN_I2C_SCL"
#endif

#if AUDIO_BUFFER_FRAMES * 1000 < AUDIO_RATE_MAX
#error "AUDIO_BUFFER_FRAMES is too small: a buffer must hold at least 1 ms of audio"
#endif

// =============================================================================
// Settings whose feature is not implemented yet
// =============================================================================

#if AUDIO_CHANNELS == 1
#error "AUDIO_CHANNELS 1 (mono) is not implemented yet"
#endif
#if I2S_ROLE == I2S_ROLE_CONTROLLER
#error "I2S_ROLE_CONTROLLER is not implemented yet"
#endif
#if CODEC == CODEC_SGTL5000
#error "CODEC_SGTL5000 is not implemented yet"
#endif
#if CODEC == CODEC_TAA5212
#error "CODEC_TAA5212 is not implemented yet"
#endif
#if CFG_PIN_SET(PIN_AVDD_EN)
#error "PIN_AVDD_EN is set, but analog power control is not implemented yet"
#endif
#if CFG_PIN_SET(PIN_POT)
#error "PIN_POT is set, but the volume pot is not implemented yet"
#endif
#if CFG_PIN_SET(PIN_MUTE_BUTTON)
#error "PIN_MUTE_BUTTON is set, but the mute button is not implemented yet"
#endif
#if CFG_PIN_SET(PIN_MUTE_LED)
#error "PIN_MUTE_LED is set, but the mute LED is not implemented yet"
#endif
#if CFG_PIN_SET(PIN_MUTE_SWITCH)
#error "PIN_MUTE_SWITCH is set, but the mute switch is not implemented yet"
#endif
#if USB_SUSPEND_POWER_DOWN
#error "USB_SUSPEND_POWER_DOWN is not implemented yet"
#endif

#endif // AUDIO_CONFIG_CHECK_H
