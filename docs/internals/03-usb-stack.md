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
itself. The clear must cover both buffer 0 and buffer 1 (shifted mask).

This callback is the **only** hook that fires on Alt 0 in TinyUSB's
`TUP_DCD_EDPT_ISO_ALLOC` code path. TinyUSB has no
`dcd_edpt_iso_deactivate` API, so the DCD layer is never notified of
stream closure. Do not move this logic to `dcd_edpt_iso_activate`
(Alt 1) — empirical testing showed that clears at activation time
silently break audio even though they eliminate the panic.

Do not duplicate this access pattern in other modules.
See `06-workarounds.md §6.2` for the full technical proof and upstream
PR requirements.

## 3.5 — Fixed-rate device and flow control

This firmware presents a single fixed sample rate. The Clock Source descriptor
in `src/usb_descriptors.c` declares the frequency control as **read-only**
(`AUDIO_CTRL_R`). Consequently, Linux's UAC2 driver (`sound/usb/clock.c`)
never issues `SET_CUR(SAM_FREQ)` — by design and per spec, hosts must not
write to a read-only control.

`tud_audio_set_req_entity_cb` in `src/usb_audio.c` returns `false` (STALL)
for all SET entity requests. This is correct: the hardware cannot change its
sample rate, so any SET attempt should be rejected.

**TinyUSB flow control is disabled** (`CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL 0`).
TinyUSB's flow control path initialises its internal `sample_rate_tx` to `0`
and has no mechanism to seed it except by receiving `SET_CUR(SAM_FREQ)`. Since
Linux never sends that command for a read-only clock, `sample_rate_tx` stays
`0` and `audiod_calc_tx_packet_sz()` computes a packet size of `0` permanently.
This is a proven structural flaw in TinyUSB, not a misconfiguration of this
project. See `06-workarounds.md §6.1` for the full proof including Linux kernel
source citations and empirical serial-spy logs.

If multi-rate support is added in the future (e.g. supporting 44.1 kHz and
48 kHz and allowing the host OS to switch between them), do the following:

1. Change the Clock Source descriptor `_ctrl` field to `AUDIO_CTRL_RW` so the
   host is permitted to issue `SET_CUR(SAM_FREQ)`.
2. Update the `GET_RANGE` handler to advertise multiple `subrange` entries
   instead of a single fixed `bMin == bMax`.
3. Implement a real SET handler in `usb_audio.c` that reconfigures the PIO
   clock divider in `i2s_audio.c` (via a new public API on that module) AND
   reconfigures the PWM MCLK divider when `GENERATE_MCLK` is set.
4. Re-evaluate `CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL` — once the host can and
   does send `SET_CUR`, the upstream fix for the `sample_rate_tx`
   initialization path would need to land in TinyUSB first, OR the project
   must seed `sample_rate_tx` via an internal patch.

Until all of the above are done, the STALL behaviour and `EP_IN_FLOW_CONTROL 0`
stay.
