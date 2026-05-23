# TinyUSB / UAC2 Contract

## R3.1 — Module boundary for the USB stack

- All `tud_audio_*` callbacks (entity GET/SET, EP GET/SET, ITF GET/SET,
  `tud_audio_tx_done_pre_load_cb`, `tud_audio_set_itf_cb`,
  `tud_audio_set_itf_close_EP_cb`) live in `src/usb_audio.c`. Do not
  implement them anywhere else.
- All USB descriptor tables, descriptor-length macros, and the
  `tud_descriptor_device_cb` / `tud_descriptor_configuration_cb` /
  `tud_descriptor_string_cb` callbacks live in `src/usb_descriptors.c`.
- TinyUSB stack configuration macros (`CFG_TUD_*`, `CFG_TUSB_*`,
  `TUD_AUDIO_*_DESC_LEN`) live in `src/tusb_config.h`.

## R3.2 — Shared USB contract constants live in `usb_descriptors.h`

Any integer that appears both in the descriptor table in
`src/usb_descriptors.c` AND in a USB callback in `src/usb_audio.c` MUST be
defined as a macro in `src/usb_descriptors.h`. This includes:

- USB endpoint addresses (e.g. `EPNUM_AUDIO_IN`).
- UAC2 entity IDs:
  - `UAC2_ENTITY_INPUT_TERMINAL`
  - `UAC2_ENTITY_FEATURE_UNIT`
  - `UAC2_ENTITY_OUTPUT_TERMINAL`
  - `UAC2_ENTITY_CLOCK_SOURCE`
- Interface numbers when referenced from both sides.
- String descriptor indices when referenced from both sides.

Bare integer literals (e.g. `0x04`, `0x81`) for these contract values are
FORBIDDEN in `usb_descriptors.c` and `usb_audio.c`. Use the macro names.

Implementation-only literals that never cross this boundary (loop bounds,
internal scratch buffer indices, the descriptor-internal `_ctrl`/`_attr`
flag bytes) stay in their owning `.c` file.

## R3.3 — Descriptor length is asserted

`src/usb_descriptors.c` contains:

```c
static_assert(sizeof(desc_configuration) == TUD_CONFIG_DESC_LEN + TUD_AUDIO_MIC_TWO_CH_DESC_LEN,
              "Descriptor length mismatch — update TUD_CONFIG_DESCRIPTOR total length");
```

Any plan that adds, removes, or reorders descriptor blocks MUST update
`TUD_AUDIO_MIC_TWO_CH_DESC_LEN` in `src/tusb_config.h` and the
`TUD_CONFIG_DESCRIPTOR` total length argument so that this `static_assert`
continues to hold. Removing the `static_assert` is forbidden.

## R3.4 — Never STALL on the RP2350

STALL responses on the RP2350 USB peripheral have been observed to lock
the hardware. Every `tud_audio_*` control callback in `src/usb_audio.c`
MUST return either:

- `true` (silent ACK for SET requests), or
- `tud_control_xfer(rhport, p_request, NULL, 0)` (zero-length ACK for GET
  requests), or
- `tud_control_xfer(...)` with real payload for supported entities.

Returning `false` from a control callback (which makes TinyUSB STALL) is
a violation. See `06-workarounds.md` R6.4 for the underlying reason.

## R3.5 — Endpoint reactivation workaround

`tud_audio_set_itf_close_EP_cb` in `src/usb_audio.c` manually clears the
`USB_BUF_CTRL_AVAIL` and `USB_BUF_CTRL_FULL` bits in
`usb_dpram->ep_buf_ctrl[ep_num].in` for the audio IN endpoint. This is
the only sanctioned direct access to `usb_dpram` outside of TinyUSB
itself. The clear MUST cover both buffer 0 and buffer 1 (shifted mask).

Do not remove this callback. Do not duplicate this access pattern in
other modules. See `06-workarounds.md` R6.5.

## R3.6 — Fixed-rate device

This firmware presents a single fixed sample rate. The `SET_CUR(SAM_FREQ)`
request from the host is intentionally swallowed by
`tud_audio_set_req_entity_cb` because the I2S ADC clock is hardware-fixed.

If multi-rate support is added in the future, the plan MUST:

1. Update `UAC2_ENTITY_CLOCK_SOURCE` GET handlers to advertise multiple
   `subrange` entries instead of a single fixed `bMin == bMax`.
2. Implement an actual SET handler that reconfigures the PIO clock divider
   in `i2s_audio.c` (via a new public API on that module) AND reconfigures
   the PWM MCLK divider when `GENERATE_MCLK` is set.
3. Re-evaluate `CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL` — the rationale for
   disabling it (see `06-workarounds.md` R6.3) may no longer apply.

Until all three are done, the swallow-SET behaviour stays.
