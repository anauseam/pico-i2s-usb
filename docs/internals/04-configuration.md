# Configuration Centralization

## 4.1 — `audio_config.h` is the single source of truth for tunables

All user-tunable values should live in `src/audio_config.h`. This includes:

- GPIO pin assignments (`PIN_DIN`, `PIN_CLOCK_BASE`, `PIN_MCLK`).
- Audio parameters: `SAMPLE_RATE`, `AUDIO_BUFFER_SIZE`.
- Mode flags: `GENERATE_MCLK`, `AUDIO_DEBUG_LOGGING`.

If a future feature exposes a tunable to the user, it goes here, not in a
module-specific header and not as a `#define` inside a `.c` file.

## 4.2 — TinyUSB stack config lives in `tusb_config.h`

All `CFG_TUD_*`, `CFG_TUSB_*`, and TinyUSB descriptor-length macros
(`TUD_AUDIO_MIC_TWO_CH_DESC_LEN`, `CFG_TUD_AUDIO_FUNC_1_DESC_LEN`, etc.)
live in `src/tusb_config.h`. Do not duplicate or override these elsewhere.

`tusb_config.h` is allowed to `#include "audio_config.h"` to derive USB
parameters from the audio parameters (e.g.
`CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX` is computed from `SAMPLE_RATE`). This
direction is fine; the reverse (`audio_config.h` including `tusb_config.h`)
is avoided.

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

Changing `SAMPLE_RATE` to another supported value (44100 / 48000 / 96000)
or remapping `PIN_DIN` / `PIN_CLOCK_BASE` / `PIN_MCLK` to other GPIOs does
NOT require updating files in `docs/internals/`. These are exactly the
kinds of tunables `audio_config.h` is meant to absorb.

The rule files only need updating when the *architecture* changes
(new module, new peripheral, new descriptor topology, lifted workaround).
