# RP2350 + TinyUSB Workarounds — DO NOT REMOVE

Every item in this file is a deliberate workaround for a **reproduced**
hardware or upstream-stack bug. They should remain in the codebase with
their accompanying block comment intact. Removing one requires the commit
message to explicitly cite the upstream fix (Pico SDK / TinyUSB version
and PR/issue number) that makes the workaround unnecessary.

For *suspected-but-unreproduced* defensive code (the `CFG_TUSB_DEBUG=0`
override, and the PIO frame-misalignment limitation), see
`suspected-issues.md`.

---

## 6.1 — `CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL = 0`

**Location:** `src/tusb_config.h`

```c
// Disable flow control — it requires sample_rate_tx to be set via SET_CUR
// before SET_INTERFACE, but Linux sends SET_INTERFACE first. With flow
// control enabled, audiod_calc_tx_packet_sz() silently fails and leaves
// packet_sz_tx at zero, making the TX path fragile.
#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL 0
```

### Root Cause (Definitively Proven)

TinyUSB's flow control for UAC2 IN endpoints (`CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL`)
is **fundamentally incompatible** with any fixed-frequency device on Linux.
This is not a suspected issue or an ordering race — it is a structural design
flaw in TinyUSB that was proven empirically in May 2026.

The failure mode has two independent, compounding causes:

#### Cause A — Linux reads `bmControls` before sending `SET_CUR`

The Linux UAC2 driver (`sound/usb/clock.c`, function `snd_usb_set_sample_rate_v2v3`)
reads the clock source's `bmControls` field from the device descriptor before
deciding whether to issue `SET_CUR(SAM_FREQ)`. The relevant kernel code (as
of Linux mainline, May 2026):

```c
// sound/usb/clock.c — snd_usb_set_sample_rate_v2v3()
writeable = uac_v2v3_control_is_writeable(bmControls,
                                          UAC2_CS_CONTROL_SAM_FREQ);
if (!writeable)
    return 0;  // Silent early return — SET_CUR is never sent.
```

This device's Clock Source descriptor declares the frequency control as
**read-only** (`AUDIO_CTRL_R`) because the I2S ADC clock is hardware-fixed:

```c
// src/usb_descriptors.c
TUD_AUDIO_DESC_CLK_SRC(
    /*_clkid*/ UAC2_ENTITY_CLOCK_SOURCE,
    /*_attr*/  AUDIO_CLOCK_SOURCE_ATT_INT_FIX_CLK,
    /*_ctrl*/  (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS),
    ...
```

Because `AUDIO_CTRL_R` means the host cannot change the frequency, Linux
correctly concludes that issuing `SET_CUR` would violate the device's declared
capabilities. It performs a `GET_CUR` (to read the current rate) and returns
without sending `SET_CUR`.

#### Cause B — Linux skips `SET_CUR` when the current rate already matches

A second, independent guard exists in `set_sample_rate_v2v3()`:

```c
// sound/usb/clock.c — set_sample_rate_v2v3()
prev_rate = get_sample_rate_v2v3(...);  // Issues GET_CUR
if (prev_rate == rate)
    goto validation;                    // Skip SET_CUR entirely.

cur_rate = snd_usb_set_sample_rate_v2v3(...);  // Only reached if rate differs
```

Even for a *writable* clock, Linux reads the current frequency first and
skips `SET_CUR` when it already matches the requested rate. Because this
device responds to `GET_CUR` with `SAMPLE_RATE` (the only rate it supports),
Linux would optimise away the `SET_CUR` on this ground alone, even if the
`bmControls` check were bypassed.

### TinyUSB's Dependency and Why It Fails

TinyUSB's flow control path (`audiod_calc_tx_packet_sz`, `audio_device.c`)
calculates the correct per-packet byte count for the isochronous IN endpoint:

```c
// (simplified) audio_device.c
static void audiod_calc_tx_packet_sz(audiod_function_t *audio) {
    // Requires sample_rate_tx to be non-zero
    audio->packet_sz_tx[...] = audio->sample_rate_tx / 1000 * ...;
}
```

`sample_rate_tx` is initialized to `0` at startup. TinyUSB's **only mechanism
to set it** is to receive a `SET_CUR(SAM_FREQ)` control request from the host.
There is no public API in TinyUSB for the application to pre-initialize
`sample_rate_tx` from its own descriptor data at startup.

Because Linux never sends `SET_CUR` (for either of the reasons above),
`sample_rate_tx` remains `0` permanently. `audiod_calc_tx_packet_sz` runs and
records a packet size of `0`. The `audiod_tx_packet_size` failsafe (an `else`
branch that falls back to dumping whatever is in the FIFO) provides a
temporary cover, but this branch violates synchronous flow control semantics.

### Empirical Evidence

Verified by adding a serial spy to `audiod_control_request` in the TinyUSB
source during development (May 2026). The spy logged every class-level control
request received from the Linux host:

```text
[USB SPY] Received Class Request: bRequest=0x02, wValue=0x0100, wIndex=0x0400  ← GET_RANGE
[USB SPY] Received Class Request: bRequest=0x02, wValue=0x0100, wIndex=0x0400  ← GET_RANGE
[USB SPY] Received Class Request: bRequest=0x01, wValue=0x0100, wIndex=0x0400  ← GET_CUR
[USB SPY] Received Class Request: bRequest=0x01, wValue=0x0100, wIndex=0x0400  ← GET_CUR
[USB SPY] Received Class Request: bRequest=0x01, wValue=0x0100, wIndex=0x0400  ← GET_CUR
[USB SPY] Received Class Request: bRequest=0x01, wValue=0x0100, wIndex=0x0400  ← GET_CUR
```

`[USB SPY] Control Complete` (which fires only for OUT/SET requests) was
**never printed**, confirming that Linux issued only `GET_CUR` and `GET_RANGE`
requests and sent no `SET_CUR` during either device enumeration or stream
activation.

A companion diagnostic print in `audiod_calc_tx_packet_sz` confirmed that
`sample_rate_tx` remained `0` throughout every `SET_INTERFACE` call.

### Why This is a TinyUSB Design Flaw

This is not a Linux bug, nor a project misconfiguration. Both behaviours
(skipping `SET_CUR` for read-only clocks, and skipping it when the current
rate already matches) are correct per the UAC2 specification (USB Audio Class 2.0,
section 5.2.5, Clock Source Descriptor `bmControls` field). A conformant host
is permitted to omit `SET_CUR` in both cases.

TinyUSB's UAC2 flow control is therefore broken for any fixed-frequency device
on Linux, and likely on any host that correctly respects read-only clock
descriptors. The failsafe (`else` dump branch) provides cover only because
it violates synchronous flow control.

An upstream fix would need to:

1. Add a call to `audiod_calc_tx_packet_sz()` inside `audiod_control_complete()`
   when `SET_CUR(SAM_FREQ)` is received (to fix the ordering for multi-rate
   devices on Linux).
2. Provide a mechanism — e.g. parse the clock source descriptor's frequency
   range at `SET_INTERFACE` time, or expose an API for the application to
   seed `sample_rate_tx` — so fixed-rate devices work correctly without
   relying on `SET_CUR` arriving at all.

### Workaround

Disabling flow control (`CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL 0`) causes TinyUSB
to bypass `audiod_calc_tx_packet_sz()` entirely and instead dump whatever
bytes are available in the FIFO each frame. This is acceptable here because
the application (`main.c`) writes exactly the right number of bytes per frame
into the FIFO via `tud_audio_write()`, so the FIFO level acts as the implicit
flow control signal. The total pipeline is:

```text
DMA IRQ  →  tud_audio_write(buf, n_bytes)  →  ep_in_ff  →  USB ISO IN
```

Because `n_bytes` is always `AUDIO_BUFFER_SIZE × sizeof(uint32_t)` and the
write rate is locked to the ADC clock, the isochronous packet payload is
always correct without TinyUSB's framework-level math.

**This workaround must be removed only when:**

1. An upstream TinyUSB version that correctly handles fixed-rate devices is
   adopted, AND
2. Multi-rate support is implemented (see `03-usb-stack.md §3.5`) such that
   `sample_rate_tx` can actually be set meaningfully.

Until both conditions are met, this define stays at `0`.

---

## 6.2 — Manual `USB_BUF_CTRL_AVAIL` / `USB_BUF_CTRL_FULL` clear

**Location:** `src/usb_audio.c`, function `tud_audio_set_itf_close_EP_cb`.

```c
uint8_t ep_num = EPNUM_AUDIO_IN & 0x7F;
uint32_t volatile *buf_ctrl = &usb_dpram->ep_buf_ctrl[ep_num].in;
uint32_t mask = USB_BUF_CTRL_AVAIL | USB_BUF_CTRL_FULL;
*buf_ctrl &= ~(mask | (mask << 16));
```

**Reason:** TinyUSB's RP2040/RP2350 DCD uses `TUP_DCD_EDPT_ISO_ALLOC`,
which pre-allocates Isochronous endpoints and skips the standard
close/reopen sequence during `SET_INTERFACE` alt-setting transitions.
After the host deactivates the stream (Alt 0), the hardware DPRAM
`USB_BUF_CTRL_AVAIL` bit remains set. When the host reactivates
(Alt 1), `_hw_endpoint_buffer_control_update32` in `rp2040_usb.c`
checks whether AVAIL is already set before arming it, and panics:

```c
// rp2040_usb.c — _hw_endpoint_buffer_control_update32
if (or_mask & USB_BUF_CTRL_AVAIL) {
    if (*ep->buffer_control & USB_BUF_CTRL_AVAIL) {
        panic("ep %02X was already available", ep->ep_addr);
    }
    ...
}
```

Manually clearing both DPRAM sub-buffers (mask and `mask << 16`)
on stream close disarms this check before the next activation.

This was reproduced concretely: the panic message ("ep 81 was already
available") was observed in UART logs across multiple Alt 0/Alt 1
transitions before the workaround.

### Why this cannot be fixed upstream without an API change

`TUP_DCD_EDPT_ISO_ALLOC` intentionally skips `dcd_edpt_close` for
Isochronous endpoints to avoid repeated allocation costs. The corollary
is that TinyUSB's DCD layer has **no `dcd_edpt_iso_deactivate` hook** —
confirmed by grepping the entire TinyUSB source tree (v2.2.0). When the
host sends an Alt 0, the DCD is never notified.

A patch to `dcd_edpt_iso_activate` (Alt 1 / re-open) is not a valid
alternative. It clears AVAIL at the wrong moment: right as the host is
expecting valid IN data, causing the hardware to skip the first packet
and stall the audio pipeline. This was verified empirically in May 2026
— removing the close-callback workaround and placing the same clear in
`dcd_edpt_iso_activate` eliminated the panic but silently broke audio.

A correct upstream PR would require:

1. Adding a `dcd_edpt_iso_deactivate(rhport, ep_addr)` API to `dcd.h`.
2. Calling it from `audio_device.c` on the `#ifdef TUP_DCD_EDPT_ISO_ALLOC`
   Alt 0 path (currently only clears the FIFO).
3. Implementing it in `dcd_rp2040.c` with the same bit-clear logic.

Until that upstream change lands, `tud_audio_set_itf_close_EP_cb` is
the only application-level hook that fires on Alt 0, making it the
correct and permanent home for this fix.

This is the only sanctioned direct write to `usb_dpram` outside of
TinyUSB itself (see `03-usb-stack.md`).
