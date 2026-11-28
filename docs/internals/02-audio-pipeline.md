# Real-Time Audio Pipeline Constraints

The audio pipeline is a hard real-time path. Violations of these rules cause
audible dropouts, USB underruns, or hardware lockup.

## 2.1 — Hot-path function inventory

The "hot path" is the set of functions that execute on every audio buffer or
every USB frame. They are:

- `dma_handler` in `src/dma_audio.c` (DMA completion ISR).
- `dma_audio_get_ready_buffer` and `dma_audio_buffer_pending` in
  `src/dma_audio.c`.
- `usb_audio_event_pending` in `src/usb_audio.c`.
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
- `printf`, even under `#if AUDIO_DEBUG_LOGGING`: it waits for the UART
  (2.3).

The only sanctioned wait in the conductor loop is the `wfi` at the bottom of
`while (1)` in `main.c`. It runs with interrupts disabled, and only when no
USB event (`usb_audio_event_pending`) and no DMA buffer
(`dma_audio_buffer_pending`) is waiting; an interrupt that arrives after that
check still ends the `wfi`. A bare `wfi` can sleep through an event that
arrived just after `usb_audio_task()` returned. Nothing else wakes the core
until the next DMA interrupt, up to one buffer later, and the isochronous IN
packets for those USB frames go unsent. Any new interrupt that leaves work for
the loop needs its own check in that condition.

## 2.3 — Diagnostic logging is gated

Diagnostic output in the loop is wrapped in `#if AUDIO_DEBUG_LOGGING` and
limited to one line about every 1.3 s (the divider in `main.c`).

Never call `printf` in the loop: the SDK waits until the whole line has left
the UART (about 87 µs per character at 115 200 baud), and the host misses an
isochronous packet for every millisecond of that. `main.c` formats the line
with `snprintf` and writes it a few characters per pass, only while
`uart_is_writable()`; new diagnostic output goes through the same buffer. For
the same reason `CFG_TUSB_DEBUG` stays 0 in `tusb_config.h`.

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
   flag. If the other channel's flag is still set, the chain has already
   restarted that channel into its buffer, so the ISR clears that flag and
   counts an overrun (`dma_audio_get_overrun_count`). At most one flag is
   therefore set at a time, and the loop always receives buffers in order.
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

## 2.6 — Rate matching is TinyUSB's flow control

Rate matching between the I2S ADC clock and the USB host clock is done by TinyUSB's flow control (`audiod_tx_packet_size` in `lib/tinyusb/src/class/audio/audio_device.c`). It ships the nominal number of frames per IN packet (48 at 48 kHz), or one frame fewer or more when the `ep_in_ff` fill level drifts below or above half the FIFO. That variation in IN packet size *is* the rate signal required by UAC2 asynchronous IN streams; the host's UAC2 driver averages packet sizes over many frames to derive the device's true sample rate. Holding the FIFO half full adds about 2048 bytes (5.3 ms at 48 kHz) of latency.

TinyUSB takes the nominal rate from the `GET_CUR(SAM_FREQ)` reply, so that reply must go through `tud_audio_buffer_and_schedule_control_xfer` (see `03-usb-stack.md §3.4`).

The application is responsible only for:

1. Writing whole, 4-byte-aligned DMA buffers to `ep_in_ff` via
   `tud_audio_write` (see 2.5).
2. Detecting overflow by querying free space in `ep_in_ff` *before* the
   write call (`tu_fifo_remaining(tud_audio_get_ep_in_ff())`), and on
   overflow, dropping the buffer entirely and incrementing a diagnostic
   counter (`usb_audio_get_overflow_count`).
3. Never attempting partial writes — partial writes split a single DMA
   buffer across two IN packets at non-sample-aligned boundaries.
4. Emptying `ep_in_ff` when the host starts streaming (`tud_audio_set_itf_cb`, any alternate setting other than 0). TinyUSB empties it only when the stream stops (`audio_device.c:1089-1098`), and the application keeps writing while the stream is stopped, so without this the first ~10 ms of every recording would be old audio.

The application should not:

- Implement an `is_streaming` pre-roll latch.
- Modulate the number of words written based on `tu_fifo_count` ("FIFO-fill hysteresis"). This is overcorrection: TinyUSB's flow control already adjusts packet size from the FIFO level.
- Maintain any sample counter for rate-feedback purposes.

These would all be application-level rate-matching code that would fight
TinyUSB's rate signal.

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
`USB_BYTES_PER_SAMPLE` in `tusb_config.h`) requires changing all three
in the same commit.
