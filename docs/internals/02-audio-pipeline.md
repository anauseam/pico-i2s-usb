# Real-Time Audio Pipeline Constraints

The audio pipeline is a hard real-time path. Violations of these rules cause
audible dropouts, USB underruns, or hardware lockup.

## 2.1 — Hot-path function inventory

The "hot path" is the set of functions that execute on every audio buffer or
every USB microframe. They are:

- `dma_handler` in `src/dma_audio.c` (DMA completion ISR).
- `dma_audio_get_ready_buffer` in `src/dma_audio.c`.
- `usb_audio_submit_buffer` in `src/usb_audio.c` (writes one DMA buffer to
  TinyUSB's `ep_in_ff`, or drops it on overflow).
- The body of `while (1)` in `src/main.c`.

There is no application-level USB hot-path callback. TinyUSB's
`audiod_tx_done_cb` (in `tinyusb/src/class/audio/audio_device.c`) drains
`ep_in_ff` on its own; this codebase does not implement
`tud_audio_tx_done_pre_load_cb` or `tud_audio_tx_done_post_load_cb`.

## 2.2 — Hot path is non-blocking

The following should be avoided inside any function listed in 2.1 to prevent stuttering:

- Heap allocation: `malloc`, `calloc`, `realloc`, `free`, `new`/`delete`.
- Blocking SDK calls: `sleep_ms`, `sleep_us`, `busy_wait_ms`,
  `busy_wait_us`, `busy_wait_at_least_cycles`.
- Synchronous file/UART/I²C/SPI blocking transfers.
- Unbounded loops on hardware status registers (any `while (!flag) {}`).
- Unconditional `printf`. Use `#if AUDIO_DEBUG_LOGGING` guards.

The only sanctioned wait in the conductor loop is `__asm volatile("wfi")` at
the bottom of `while (1)` in `main.c`. Do not remove it.

## 2.3 — Diagnostic logging is gated

Every `printf` in a hot-path function should be wrapped in
`#if AUDIO_DEBUG_LOGGING ... #endif` and emitted at a human-readable rate.
The current pattern is a modulo divider on a buffer counter (≈ 1.3 s
between prints in `main.c`). Maintain that pattern: per-buffer or
per-microframe prints will disrupt the timing.

One-shot prints during `*_init` functions are allowed and need no guard;
init runs once, before streaming.

## 2.4 — DMA ping-pong contract

The DMA layer in `src/dma_audio.c` follows this contract; it should be
preserved:

1. Two DMA channels (`dma_chan_a`, `dma_chan_b`) are claimed at init.
2. Each channel's config chains to the other (`channel_config_set_chain_to`).
3. Both channels are armed on `DMA_IRQ_0`.
4. The single ISR `dma_handler` checks `dma_hw->ints0`, clears the matching
   bit, re-arms the just-completed channel (resetting write address and
   transfer count), and sets the corresponding `volatile bool buffer_*_ready`
   flag.
5. `__dmb()` memory barriers MUST surround writes to `buffer_*_ready` in the
   ISR and reads/clears of the same flags in `dma_audio_get_ready_buffer`.
6. The two static buffers `audio_buffer_a` and `audio_buffer_b` are
   `AUDIO_BUFFER_SIZE` 32-bit words each and live at file scope in
   `dma_audio.c`.

Changing buffer count, IRQ number, or removing the `__dmb()` barriers will likely break the pipeline.

## 2.5 — Single application-owned SPSC hand-off

There is exactly ONE application-owned SPSC hand-off in this codebase:
between `dma_handler` (producer, ISR context) and the conductor loop in
`main.c` (consumer). Its contract is fully specified by 2.4 — the
`buffer_*_ready` flags, the channel re-arm sequence, and the `__dmb()`
barriers.

No additional application-level ring buffer or FIFO should be used between
`dma_audio` and TinyUSB. The audio path is:

```text
dma_audio (2.4) → main.c (conductor) → tud_audio_write → ep_in_ff
```

TinyUSB's `ep_in_ff` is owned by TinyUSB; its concurrency model (internal
mutex when `CFG_FIFO_MUTEX` is on, byte-granular write semantics) is not
the application's contract to enforce. The application's responsibility
ends at `tud_audio_write` with a 4-aligned byte length.

Adding a second application-side FIFO between the DMA and TinyUSB is highly discouraged, as it can introduce byte-alignment hazards that cause audible distortion.

## 2.6 — Rate matching is implicit via `ep_in_ff` packet-size variation

Rate matching between the I2S ADC clock and the USB host clock is
performed implicitly by TinyUSB's `audiod_tx_done_cb`, which calls
`tu_min16(tu_fifo_count, ep_in_sz)` to decide how many bytes to ship per
IN packet (in `tinyusb/src/class/audio/audio_device.c`). The resulting
variation in IN packet size *is* the rate signal required by UAC2
asynchronous IN streams; the host's UAC2 driver averages these packet
sizes over many frames to derive the device's true sample rate.

The application is responsible only for:

1. Writing whole, 4-byte-aligned DMA buffers to `ep_in_ff` via
   `tud_audio_write` (see 2.5).
2. Detecting overflow by querying free space in `ep_in_ff` *before* the
   write call (`tu_fifo_remaining(tud_audio_get_ep_in_ff())`), and on
   overflow, dropping the buffer entirely and incrementing a diagnostic
   counter (`usb_audio_get_overflow_count`).
3. Never attempting partial writes — partial writes split a single DMA
   buffer across two IN packets at non-sample-aligned boundaries.

The application should not:

- Implement an `is_streaming` pre-roll latch.
- Modulate the number of words written based on `tu_fifo_count`
  ("FIFO-fill hysteresis"). This is overcorrection: TinyUSB already
  varies packet size by virtue of how the FIFO drains.
- Maintain any sample counter for rate-feedback purposes.

These would all be application-level rate-matching code that would fight
TinyUSB's implicit rate signal.

If a future host is observed to *not* properly average packet sizes
(i.e. it requires an explicit feedback EP), please avoid adding an application-side
workaround for rate matching. The preferred solution would be to add feedback-EP-for-IN
support to the USB stack instead.

## 2.7 — Sample format is fixed by descriptors

The audio path treats each PIO word as a 32-bit container with 24 bits of
audio in the upper bits. No bit-shifting or repacking happens in the C
code — the UAC2 descriptors in `usb_descriptors.c` tell the host how to
interpret the container (`_nBytesPerSample=4`, `_nBitsUsedPerSample=24`).

Changing one side (PIO shift count, descriptor sample size, or
`CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX`) requires changing all three
in the same commit.
