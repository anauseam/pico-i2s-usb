# TinyUSB / UAC2 Contract

## 3.1 — Module boundary for the USB stack

- All `tud_audio_*` callbacks live in `src/usb_audio.c`. Do not implement them anywhere else.
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

## 3.4 — Fixed-rate device and flow control

This firmware presents a single fixed sample rate. The Clock Source descriptor in `src/usb_descriptors.c` declares the frequency control **read-only** (`AUDIO20_CTRL_R`), so Linux's UAC2 driver (`sound/usb/clock.c`) never issues `SET_CUR(SAM_FREQ)`.

`tud_audio_set_req_entity_cb` in `src/usb_audio.c` returns `false` (STALL)
for all SET entity requests. This is correct: the hardware cannot change its
sample rate, so any SET attempt should be rejected.

**TinyUSB flow control is on** (its default). It sizes packets from the sample rate it stores, which it takes from `SET_CUR(SAM_FREQ)` (`audio_device.c:1306-1311`) or from a `GET_CUR(SAM_FREQ)` reply sent through `tud_audio_buffer_and_schedule_control_xfer` (`:1712-1720`). Linux never sends `SET_CUR` to this device: its UAC2 driver skips writes to a read-only clock, and skips them when `GET_CUR` already returns the requested rate (`sound/usb/clock.c`, `snd_usb_set_sample_rate_v2v3()` and `set_sample_rate_v2v3()`). A serial spy in `audiod_control_request` confirmed that only `GET_RANGE` and `GET_CUR` arrive. So every reply in `tud_audio_get_req_entity_cb` goes through `tud_audio_buffer_and_schedule_control_xfer`; a reply through `tud_control_xfer` would leave the rate at `0`, and packets would fall back to whatever the FIFO holds (`:1893-1894`).

If multi-rate support is added in the future (e.g. supporting 44.1 kHz and
48 kHz and allowing the host OS to switch between them), do the following:

1. Change the Clock Source descriptor `_ctrl` field to `AUDIO20_CTRL_RW` so the host is permitted to issue `SET_CUR(SAM_FREQ)`.
2. Update the `GET_RANGE` handler to advertise multiple `subrange` entries
   instead of a single fixed `bMin == bMax`.
3. Implement a real SET handler in `usb_audio.c` that reconfigures the PIO
clock divider in `i2s_audio.c`.

Until all of the above are done, the STALL behaviour stays.
