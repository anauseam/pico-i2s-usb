# TinyUSB / UAC2 Contract

## 3.1 — Module boundary for the USB stack

- All `tud_audio_*` callbacks (entity GET/SET, EP GET/SET, ITF GET/SET,
  `tud_audio_tx_done_pre_load_cb`, `tud_audio_set_itf_cb`,
  `tud_audio_set_itf_close_EP_cb`) live in `src/usb_audio.c`. Do not
  implement them anywhere else.
- All USB descriptor tables, descriptor-length macros, and the
  `tud_descriptor_device_cb` / `tud_descriptor_configuration_cb` /
  `tud_descriptor_string_cb` callbacks live in `src/usb_descriptors.c`.
- TinyUSB stack configuration macros (`CFG_TUD_*`, `CFG_TUSB_*`,
  `TUD_AUDIO_*_DESC_LEN`) live in `src/tusb_config.h`.

## 3.2 — Shared USB contract constants live in `usb_descriptors.h`

Any integer that appears both in the descriptor table in
`src/usb_descriptors.c` AND in a USB callback in `src/usb_audio.c` should be
defined as a macro in `src/usb_descriptors.h`. This includes:

- USB endpoint addresses (e.g. `EPNUM_AUDIO_IN`).
- UAC2 entity IDs:
  - `UAC2_ENTITY_INPUT_TERMINAL`
  - `UAC2_ENTITY_FEATURE_UNIT`
  - `UAC2_ENTITY_OUTPUT_TERMINAL`
  - `UAC2_ENTITY_CLOCK_SOURCE`
- Interface numbers when referenced from both sides.
- String descriptor indices when referenced from both sides.

Bare integer literals (e.g. `0x04`, `0x81`) for these contract values should
be avoided in `usb_descriptors.c` and `usb_audio.c`. Use the macro names.

Implementation-only literals that never cross this boundary (loop bounds,
internal scratch buffer indices, the descriptor-internal `_ctrl`/`_attr`
flag bytes) stay in their owning `.c` file.

## 3.3 — Descriptor length is asserted

`src/usb_descriptors.c` contains:

```c
static_assert(sizeof(desc_configuration) == TUD_CONFIG_DESC_LEN + TUD_AUDIO_MIC_TWO_CH_DESC_LEN,
              "Descriptor length mismatch — update TUD_CONFIG_DESCRIPTOR total length");
```

Any plan that adds, removes, or reorders descriptor blocks must update
`TUD_AUDIO_MIC_TWO_CH_DESC_LEN` in `src/tusb_config.h` and the
`TUD_CONFIG_DESCRIPTOR` total length argument so that this `static_assert`
continues to hold.

## 3.4 — Endpoint reactivation workaround

`tud_audio_set_itf_close_EP_cb` in `src/usb_audio.c` manually clears the
`USB_BUF_CTRL_AVAIL` and `USB_BUF_CTRL_FULL` bits in
`usb_dpram->ep_buf_ctrl[ep_num].in` for the audio IN endpoint. This is
the only sanctioned direct access to `usb_dpram` outside of TinyUSB
itself. The clear should cover both buffer 0 and buffer 1 (shifted mask).

This callback is necessary. Do not duplicate this access pattern in
other modules. See `06-workarounds.md` (proven workaround).

## 3.5 — Fixed-rate device

This firmware presents a single fixed sample rate. The `SET_CUR(SAM_FREQ)`
request from the host is intentionally swallowed by
`tud_audio_set_req_entity_cb` because the I2S ADC clock is hardware-fixed.

If multi-rate support (e.g. supporting both 44.1kHz and 48kHz and allowing the host OS to switch between them) is added in the future, we should:

1. Update `UAC2_ENTITY_CLOCK_SOURCE` GET handlers to advertise multiple
   `subrange` entries instead of a single fixed `bMin == bMax`.
2. Implement an actual SET handler that reconfigures the PIO clock divider
   in `i2s_audio.c` (via a new public API on that module) AND reconfigures
   the PWM MCLK divider when `GENERATE_MCLK` is set.
3. Re-evaluate `CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL` — the rationale for
   disabling it (see `06-workarounds.md`)
   may no longer apply.

Until all three are done, the swallow-SET behaviour stays.
