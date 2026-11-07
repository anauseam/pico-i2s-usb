# Configuration Centralization

## 4.1 — `audio_config.h` is the single source of truth for tunables

All user-tunable values should live in `src/audio_config.h`. This includes:

- GPIO pin assignments (`PIN_I2S_DIN`, `PIN_I2S_BCLK`, `PIN_I2S_LRCK`,
  `PIN_MCLK`, and the pins of the codec and the controls).
- Clocking choices: `I2S_ROLE`, `MCLK_SOURCE`.
- Audio parameters: `AUDIO_CHANNELS`, `AUDIO_RATES`, `AUDIO_BUFFER_FRAMES`.
- The codec choice: `CODEC`.
- Mode flags: the `MUTE_*` polarities, `USB_SUSPEND_POWER_DOWN`,
  `AUDIO_DEBUG_LOGGING`.

If a future feature exposes a tunable to the user, it goes here, not in a
module-specific header and not as a `#define` inside a `.c` file.

Facts about the board or the chip are not settings. Reserved pins, console
pins and the chip package come from the board header that `PICO_BOARD` selects.

A new setting takes the same form as the existing ones:

- Its default sits in an `#ifndef` guard, so a build can preset it.
- A named choice gets value names numbered from 1. `#if` reads a misspelt name
  as 0, so the check for known values rejects it.
- A pin that may be absent defaults to `PIN_NONE`.
- A setting whose feature does not exist yet stops the build with a "not
  implemented yet" `#error`. The change that implements the feature removes
  that check.

## 4.2 — TinyUSB stack config lives in `tusb_config.h`

All `CFG_TUD_*`, `CFG_TUSB_*`, and TinyUSB descriptor-length macros
(`TUD_AUDIO_MIC_TWO_CH_DESC_LEN`, `CFG_TUD_AUDIO_FUNC_1_DESC_LEN`, etc.)
live in `src/tusb_config.h`. Do not duplicate or override these elsewhere.

`tusb_config.h` is allowed to `#include "audio_config.h"` to derive USB
parameters from the audio parameters (e.g.
`CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX` is computed from `AUDIO_RATE_MAX` and
`AUDIO_CHANNELS`). This direction is fine; the reverse (`audio_config.h`
including `tusb_config.h`) is avoided.

## 4.3 — No magic numbers across translation units

A numeric literal that appears in two or more `.c` files should be promoted
to a `#define` in an appropriate shared header:

- USB contract values → `src/usb_descriptors.h`.
- Audio / pipeline tunables → `src/audio_config.h`.
- TinyUSB stack tunables → `src/tusb_config.h`.

Single-file literals (loop bounds, scratch indices, register bit positions
used only inside one `.c`) may stay as literals in that file.

When in doubt, prefer promoting to a named constant.

## 4.4 — Sample rate and pin changes do not require rule updates

Changing `AUDIO_RATES` to another supported value or remapping the `PIN_*`
settings to other GPIOs does NOT require updating files in `docs/internals/`.
These are exactly the kinds of tunables `audio_config.h` is meant to absorb.

The rule files only need updating when the *architecture* changes
(new module, new peripheral, new descriptor topology, lifted workaround).

## 4.5 — Derived values and checks have their own headers

`audio_config.h` ends by including two headers, in this order. Other files
include `audio_config.h`, never these two directly.

`audio_config_derived.h` computes values from the settings: `AUDIO_BUFFER_SIZE`,
`AUDIO_RATE_COUNT`, `AUDIO_RATE_MAX` and `SAMPLE_RATE`. These are not settings,
so they have no `#ifndef` guards.

`audio_config_check.h` stops the build with an `#error` that names the setting
to change. It checks that:

- Every setting holds a known value.
- Each in-use pin can do its job: a GPIO of this chip, a clock-output pin for
  MCLK, matching I2C pins, an ADC pin for the pot.
- No in-use pin clashes with the board, the debug console or another pin.
- Settings don't conflict: a pin-strapped ADC lists one rate, 44.1 kHz needs an
  external MCLK, the controller role needs the Pico's MCLK, a choice that needs
  a pin has it set, and a DMA buffer holds at least 1 ms of audio.
- No setting asks for a feature that is not implemented yet.

The check that a DMA buffer fits in the USB FIFO is in `tusb_config.h`, which
defines the FIFO size.

## 4.6 — The build matrix covers every setting

`scripts/build-matrix.sh` builds every supported configuration and confirms that
each bad configuration stops the build with its expected message. It passes
settings through the `AUDIO_CONFIG_DEFINES` CMake variable, so the source tree
is never edited. A build that reads an undefined name in an `#if` in `src/`
also fails, which catches a renamed setting.

CI runs the matrix on every push and pull request. A change that adds a setting,
a check, or an `#if` on a setting adds the matching cases.
