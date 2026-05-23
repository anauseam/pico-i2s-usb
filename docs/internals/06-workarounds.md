# RP2350 + TinyUSB Workarounds — DO NOT REMOVE

Every item in this file is a deliberate workaround for a **reproduced**
hardware or upstream-stack bug. They should remain in the codebase with
their accompanying block comment intact. Removing one requires the commit
message to explicitly cite the upstream fix (Pico SDK / TinyUSB version
and PR/issue number) that makes the workaround unnecessary.

For *suspected-but-unreproduced* defensive code (the `CFG_TUSB_DEBUG=0`
override, the `tu_static` alignment override, STALL avoidance, and the
PIO frame-misalignment limitation), see
`suspected-issues.md`.

## 6.1 — `CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL = 0`

Location: `src/tusb_config.h`

```c
#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL 0
```

Reason: TinyUSB's flow-control path in `audiod_calc_tx_packet_sz()`
requires the host to have sent `SET_CUR(SAM_FREQ)` before
`SET_INTERFACE`. The Linux UAC2 driver sends them in the opposite
order, so `packet_sz_tx` remains zero and the IN path silently breaks
(no audio reaches the host). Disabling flow control bypasses this
dependency.

This was reproduced concretely against the Linux UAC2 driver during
development.

If `03-usb-stack.md` (multi-rate support) is ever
implemented, this workaround should be re-evaluated.

## 6.2 — Manual `USB_BUF_CTRL_AVAIL` / `USB_BUF_CTRL_FULL` clear

Location: `src/usb_audio.c`, function `tud_audio_set_itf_close_EP_cb`.

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
both DPRAM sub-buffers (mask and `mask << 16`) restores the endpoint
to a sane state.

This was reproduced concretely: the panic message ("ep 81 was already
available") is emitted by TinyUSB's DCD and was observed in UART logs
across multiple Alt 0/Alt 1 transitions before the workaround.

This is the only sanctioned direct write to `usb_dpram` outside of
TinyUSB itself (see `03-usb-stack.md`).
