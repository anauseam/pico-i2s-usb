# RP2350 + TinyUSB Workarounds — DO NOT REMOVE

Every item in this file is a deliberate workaround for a known hardware
or upstream-stack bug. They MUST remain in the codebase with their
accompanying block comment intact. Removing one requires the commit
message to explicitly cite the upstream fix (Pico SDK / TinyUSB version
and PR/issue number) that makes the workaround unnecessary.

A plan that "cleans up" or "simplifies" any of these without that
evidence is a violation.

## R6.1 — `CFG_TUSB_DEBUG` is forced to 0

Location: `src/tusb_config.h`

```c
#ifdef CFG_TUSB_DEBUG
#undef CFG_TUSB_DEBUG
#endif
#define CFG_TUSB_DEBUG 0 // Disable debug logging on UART to prevent audio bottleneck
```

Reason: With `CFG_TUSB_DEBUG >= 1`, TinyUSB emits per-transfer log
output over UART. This blocks `tud_task()` long enough to starve the
audio stream — audio arrives at a fraction of its expected rate in
host applications (verified in Audacity). The explicit `#undef` first
defends against the SDK or build system pre-defining a non-zero value.

## R6.2 — `tu_static` alignment override

Location: `src/tusb_config.h`

```c
#undef tu_static
#define tu_static static __attribute__((aligned(4)))
```

Reason: TinyUSB declares several internal byte arrays (e.g.
`ctrl_buf_1`) via `tu_static`. GCC does not align `uint8_t` arrays by
default. On the RP2350 RISC-V cores, `tu_memcpy_s` issues 32-bit
load/store instructions during UAC2 control requests (such as
`SET_CUR`), which fault on unaligned addresses. Forcing 4-byte
alignment makes these accesses safe.

## R6.3 — `CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL = 0`

Location: `src/tusb_config.h`

```c
#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL 0
```

Reason: TinyUSB's flow-control path in `audiod_calc_tx_packet_sz()`
requires the host to have sent `SET_CUR(SAM_FREQ)` before
`SET_INTERFACE`. The Linux UAC2 driver sends them in the opposite
order, so `packet_sz_tx` remains zero and the IN path silently breaks.
Disabling flow control bypasses this dependency.

If R3.6 (multi-rate support) is ever implemented, this workaround
should be re-evaluated.

## R6.4 — STALL avoidance in UAC2 control callbacks

Location: `src/usb_audio.c`

All `tud_audio_*_req_*_cb` functions either return `true` or call
`tud_control_xfer(rhport, p_request, NULL, 0)` to ACK with zero
length. They never return `false`.

Reason: STALL responses on the RP2350 USB peripheral have not been
verified safe and have been associated with hardware lockup. UAC2
semantically expects STALL for unsupported features, but a
zero-length ACK is accepted by all tested hosts (Linux, macOS,
Windows 11) and avoids the lockup.

## R6.5 — Manual `USB_BUF_CTRL_AVAIL`/`USB_BUF_CTRL_FULL` clear

Location: `src/usb_audio.c`, function `tud_audio_set_itf_close_EP_cb`

```c
uint8_t ep_num = EPNUM_AUDIO_IN & 0x7F;
uint32_t volatile *buf_ctrl = &usb_dpram->ep_buf_ctrl[ep_num].in;
uint32_t mask = USB_BUF_CTRL_AVAIL | USB_BUF_CTRL_FULL;
*buf_ctrl &= ~(mask | (mask << 16));
```

Reason: TinyUSB's RP2040/RP2350 DCD uses `TUP_DCD_EDPT_ISO_ALLOC`,
which pre-allocates Isochronous endpoints and skips the standard
close/reopen sequence during `SET_INTERFACE` alt-setting transitions.
After the host deactivates the stream (Alt 0), the hardware DPRAM
`USB_BUF_CTRL_AVAIL` bit remains set. When the host reactivates
(Alt 1), the DCD attempts to set the bit again and panics
("ep 81 was already available"). Manually clearing the bits for
both DPRAM sub-buffers (mask and `mask << 16`) restores the
endpoint to a sane state.

This is the only sanctioned direct write to `usb_dpram` outside of
TinyUSB itself (see R3.5).

## R6.6 — PWM-MCLK sample-rate warning

Location: `src/i2s_audio.c`

```c
#if GENERATE_MCLK && (SAMPLE_RATE > 16000)
#warning "PWM MCLK above 16kHz is empirically unreliable on RP2350 ..."
#endif
```

Reason: PWM MCLK at higher sample rates exhibits fractional-divider
jitter that some ADCs cannot tolerate. The PCM1808 has been validated
up to 96 kHz despite this, but other ADCs may fail. The `#warning`
documents the trade-off without blocking compilation.

Do not remove the `#warning`. Tightening the threshold (e.g. to
`> 48000`) is acceptable if accompanied by evidence; loosening it
(e.g. to `> 96000`) requires evidence from another ADC family.

## R6.7 — PIO frame-misalignment recovery is a known gap

Location: `src/i2s_audio.c` (block comment in `i2s_audio_init`,
target-mode branch)

There is currently no watchdog or recovery path for PIO frame
misalignment after a BCLK/LRCK glitch. This is documented in code as
an "ARCHITECTURAL LIMITATION" comment. Plans that touch
`i2s_audio.c` MUST preserve this comment until an actual recovery
mechanism is implemented.

Implementing a recovery path is welcome and would consist of:

1. A watchdog timer that checks PIO RX FIFO progress.
2. A re-sync routine that disables/resets the SM and re-runs the
   initial LRCK-edge sync sequence (see the labels `wait_lrck_high`,
   `wait_lrck_low`, `wait_bclk_high_sync`, `wait_bclk_low_sync` in
   `i2s_rx_target.pio`).

Such a plan would update this section and remove the
"ARCHITECTURAL LIMITATION" comment from the source.
