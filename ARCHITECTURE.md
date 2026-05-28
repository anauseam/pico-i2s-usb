# ARCHITECTURE.md

This document is the design-rationale companion to the [README](README.md).
The README answers "how do I use this?"; this file answers "why is it built
this way?". For technical constraints and hardware contracts,
see [`docs/internals/`](docs/internals/). For descriptive notes on
unreproduced defensive code, see
[`docs/internals/suspected-issues.md`](docs/internals/suspected-issues.md).

## 1. Why this firmware exists

A driverless UAC2 microphone has three hard constraints:

1. **Real-time data path.** Audio samples are produced by the ADC at a
   fixed wall-clock rate (e.g. 48 000 stereo 24-bit-in-32-bit-container
   samples per second). Any stall longer than the USB host's tolerance
   produces an audible artifact.
2. **Standard-compliant USB descriptors.** The host's UAC2 driver
   interprets the device through its descriptors. Errors there cause
   silent enumeration failures or wrong sample-rate interpretation —
   the kind of bug that is hardest to diagnose because nothing crashes.
3. **No host-side software.** The point of UAC2 is to be a class-compliant
   device. Any code path that requires a configuration utility on the
   host defeats the purpose.

The RP2350 + TinyUSB stack is well-suited to all three, with two
caveats: the RP2350 USB peripheral has documented oddities around
isochronous-endpoint reactivation, and TinyUSB's UAC2 implementation
makes assumptions about host control-request ordering that the Linux
UAC2 driver violates. Both are handled by the workarounds documented
in [`docs/internals/06-workarounds.md`](docs/internals/06-workarounds.md).

## 2. Data pipeline narrative

The pipeline has four owners, each with a single responsibility:

```text
PCM1808-class ADC
    │
    │  I2S (BCLK + LRCK from the ADC; the Pico is the I2S Target)
    ▼
PIO state machine in src/i2s_rx_target.pio
    │
    │  one 32-bit word per L/R sample container
    │  (24-bit audio in the upper bits; lower bits are ADC noise)
    ▼
DMA ping-pong in src/dma_audio.c
    │  two static buffers, AUDIO_BUFFER_SIZE words each;
    │  ISR re-arms and sets buffer_*_ready (volatile + __dmb)
    ▼
Conductor loop in src/main.c
    │  polls dma_audio_get_ready_buffer; on each ready buffer:
    │  usb_audio_submit_buffer(buf, AUDIO_BUFFER_SIZE)
    ▼
TinyUSB ep_in_ff (4096 bytes; src/tusb_config.h sets the size)
    │
    │  Isochronous IN, host polls every 1 ms;
    │  packet size varies per microframe — that is the UAC2 rate signal
    ▼
USB host (Linux / macOS / Windows 11)
```

Three properties are worth pulling out:

- **One application-owned SPSC hand-off.** The only application-side
  producer/consumer ring is the `buffer_*_ready` flag pair between the
  DMA ISR (producer) and the conductor loop (consumer). Once a buffer
  is given to `tud_audio_write`, it is owned by TinyUSB. There is no
  second application FIFO between `dma_audio` and TinyUSB. This is
  binding per [`docs/internals/02-audio-pipeline.md`](docs/internals/02-audio-pipeline.md).
- **No application-side rate matching.** TinyUSB's `audiod_tx_done_cb`
  decides how many bytes to ship per IN packet by reading the current
  `ep_in_ff` fill level. The resulting variation in packet size is the
  rate signal that UAC2 async-IN streams use. The application does not
  modulate the number of bytes it writes per DMA buffer; that would
  fight TinyUSB's implicit signal. See
  [`docs/internals/02-audio-pipeline.md`](docs/internals/02-audio-pipeline.md).
- **No partial writes.** If TinyUSB's `ep_in_ff` does not have room for
  one full DMA buffer, the conductor drops the buffer entirely and
  increments `overflow_count`. A partial write would split a 32-bit
  sample container across two IN packets at a byte-aligned-but-not-
  word-aligned boundary, producing audible distortion. This was the
  bug that motivated the no-application-FIFO refactor.

## 3. Design decisions and why

### 3.1 Why the application does not own a FIFO

An earlier iteration of this firmware kept a software ring buffer
(`app_fifo`) in `usb_audio.c` between the DMA ISR and TinyUSB. The
intent was rate smoothing: write whole DMA buffers into `app_fifo`,
then drain `app_fifo` into TinyUSB's `ep_in_ff` byte-by-byte from the
USB callback.

This introduced a byte/word alignment hazard. The DMA buffer is a
sequence of 32-bit sample containers, but the FIFO drain wrote `min(
free, available)` bytes per call. When the free-space and available-
bytes counts happened not to be 4-aligned, the FIFO would emit a
boundary-misaligned packet. The host's UAC2 driver would reassemble
the stream byte-by-byte from those packets — and would silently shift
the channel alignment by one or more bytes on every misaligned
boundary, producing a distinctive "swappy" audible distortion.

The fix that eliminated this class of bug is structural: there is no
intermediate FIFO. The DMA buffer is written in one call to
`tud_audio_write` with a byte length that is provably 4-aligned (it
is `AUDIO_BUFFER_SIZE * 4`). TinyUSB's own FIFO is large enough
(4096 bytes, several DMA buffers' worth) to absorb timing jitter
without needing a second layer.

The trade is "drops on overflow vs. distortion on misalignment", and
the project chooses drops. The `overflow_count` diagnostic exists to
make drops visible.

See [`docs/internals/02-audio-pipeline.md`](docs/internals/02-audio-pipeline.md).

### 3.2 Why rate matching is implicit

The ADC produces samples at exactly its hardware clock rate. The host
polls the device once per USB frame (1 ms, since this is Full-Speed
USB). If the two rates were identical, exactly `SAMPLE_RATE / 1000`
samples would arrive in `ep_in_ff` between every two host polls and
the device could ship a fixed-size packet every microframe. They are
not identical (the host's USB clock and the device's I2S clock are
free-running with respect to each other), so the device must indicate
its actual rate to the host.

UAC2 supports two ways to do this. One is an explicit feedback
endpoint, where the device sends a sample-rate estimate to the host
on a separate IN endpoint. The other is implicit feedback through the
data IN endpoint itself: by varying the packet size from frame to
frame, the device communicates "I currently have N samples ready",
which the host averages over many frames to derive the true rate.

This firmware uses the second method. TinyUSB's `audiod_tx_done_cb`
ships `min(tu_fifo_count, ep_in_sz)` bytes per IN packet. The packet
size therefore *is* the device's instantaneous estimate of its own
rate. No application-side code is involved.

Implementing the first method (explicit feedback EP) would require
upstream TinyUSB support — currently TinyUSB supports feedback
endpoints for OUT streams only. If a future host is observed to
mishandle implicit feedback, the correct fix is to contribute
feedback-EP-for-IN support to TinyUSB, not to add a workaround on
the application side.

See [`docs/internals/02-audio-pipeline.md`](docs/internals/02-audio-pipeline.md).

### 3.3 Why the Pico is always Target

The README's clock-architecture matrix shows that all four
combinations of (Pico=Controller/Target × MCLK=PWM/external) were
tested, and the two Pico-as-Controller configurations fail. Both
failures have the same root cause: BCLK and LRCK must be derived
from MCLK in a phase-coherent manner, and the Pico cannot
synchronize its PIO state machine to either an external oscillator
(physically independent clock domain) or its own PWM peripheral
(independent fractional divider on the same PLL — accumulates phase
error).

The ADC, by contrast, has a single PLL that derives BCLK and LRCK
from its MCLK input by integer division. They are phase-coherent by
construction. So the ADC must own the BCLK/LRCK timebase. The Pico
sits downstream and samples the I2S data on those clock edges.

The file `src/i2s_rx_controller.pio` is retained on disk because it
documents what a Pico-as-Controller PIO program looks like, but it
is not compiled. The build system removes it from `add_executable`.
See [`docs/internals/04-configuration.md`](docs/internals/04-configuration.md).

## 4. Open observations

These are concerns logged in the maintainer's notes that have not
risen to the level of reproducible bugs. They live here rather than
in the rule files because acting on them would be premature.

### 4.1 The Pico stream rate may be slightly slower than off-the-shelf USB ADCs

User reports suggest that when the Pico is the recording source for a
session also captured by an off-the-shelf USB audio interface, the
Pico's recording is microscopically shorter on the timeline. Hypotheses
in rough order of likelihood:

1. The Pico's PLL has a small ppm offset from the host's USB clock,
   and the host's UAC2 driver resamples the device's stream to match
   its own clock — slightly stretching or compressing the timeline.
2. The host averages packet sizes over a window long enough that the
   device's true rate is determined slightly off. Different hosts
   would do this differently.
3. The device's implicit feedback signal is noisy enough that some
   hosts react conservatively and round the rate down.

None of these are confirmed. Until a measurement campaign
distinguishes between them, no code change is warranted.

### 4.2 PIO frame-misalignment recovery is unimplemented

The PIO target-mode program performs an LRCK-edge sync sequence on
init. After init, the program runs in steady-state with no further
alignment checks. A BCLK or LRCK glitch during streaming (brief
power-supply dropout to the ADC, ESD event) could leave the SM
offset by 1–31 bits, producing corrupted samples until the device
is power-cycled.

This is a known architectural gap, not a bug to fix. Implementing
recovery would require a watchdog timer on the PIO RX FIFO and a
re-sync routine. The complexity is not justified for the use cases
this firmware is designed for.

See
[`docs/internals/suspected-issues.md#pio-frame-misalignment-recovery`](docs/internals/suspected-issues.md#pio-frame-misalignment-recovery).

## 5. Pointers — where to look for what

| If you want to know…                                  | Look at…                                                                   |
| ----------------------------------------------------- | -------------------------------------------------------------------------- |
| how to build / wire / install                         | [README.md](README.md)                                                     |
| how to set the sample rate, pin map, debug flag, MCLK | [`src/audio_config.h`](src/audio_config.h)                                 |
| what the real-time constraints are                    | [`docs/internals/`](docs/internals/)                                       |
| what defensive code is in place and why (suspected)   | [`docs/internals/suspected-issues.md`](docs/internals/suspected-issues.md) |
| how to contribute (human or AI)                       | [CONTRIBUTING.md](CONTRIBUTING.md)                                         |
| the licensed terms                                    | [LICENSE](LICENSE)                                                         |
