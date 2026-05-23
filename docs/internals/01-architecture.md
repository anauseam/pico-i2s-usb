# Architecture & Module Boundaries

## 1.1 — `main.c` is the only conductor

`src/main.c` is the **only** translation unit allowed to call functions from
more than one of the audio/USB modules. Specifically, only `main.c` may call
functions from two or more of:

- `i2s_audio.h`
- `dma_audio.h`
- `usb_audio.h`
- `usb_descriptors.h`

To avoid spaghetti architecture, please do not introduce orchestration logic (start/stop, buffer hand-off,
state-machine sequencing) anywhere other than `src/main.c`.

## 1.2 — Strict peripheral ownership

Each hardware peripheral is owned by exactly one module. No other module may
touch its registers, claim its channels, install its IRQ handlers, or call its
SDK setup functions.

| Peripheral / SDK area                               | Owner module                            |
| --------------------------------------------------- | --------------------------------------- |
| PIO state machines (`pio0`, `pio1`)                 | `src/i2s_audio.{c,h}`                   |
| `hardware_pwm` (when used for MCLK)                 | `src/i2s_audio.{c,h}`                   |
| `clock_get_hz(clk_sys)` / clock config              | `src/i2s_audio.{c,h}`                   |
| DMA channels, `DMA_IRQ_0`, `dma_hw->ints0`          | `src/dma_audio.{c,h}`                   |
| TinyUSB stack init (`tusb_init`, `tud_task`)        | `src/usb_audio.{c,h}`                   |
| All `tud_audio_*` callbacks                         | `src/usb_audio.{c,h}`                   |
| All USB descriptor tables and `tud_descriptor_*_cb` | `src/usb_descriptors.{c,h}`             |
| RP2350 USB DPRAM access (`usb_dpram->...`)          | `src/usb_audio.{c,h}` (workaround only) |

Adding a new peripheral requires a new module pair (`<name>.c` + `<name>.h`)
plus a new entry in `CMakeLists.txt`. Do not extend an existing module to
cover a second peripheral.

## 1.3 — No cross-module header includes

A `.c` file may include:

- Its own `.h`.
- `audio_config.h`.
- `usb_descriptors.h` (only from `usb_audio.c` — for shared USB contract IDs).
- Pico SDK / TinyUSB headers.
- C standard library headers.

A `.c` file should avoid including another module's header just to call into it.
Inter-module data hand-off goes through `main.c`:

- `i2s_audio_init` returns `PIO` + `sm` to `main.c`.
- `main.c` passes them to `dma_audio_init`.
- `dma_audio_get_ready_buffer` returns a buffer pointer to `main.c`.
- `main.c` passes that pointer to `usb_audio_send_buffer`.

This is the only sanctioned data-flow topology.

## 1.4 — Headers are guarded and minimal

Every `.h` file in `src/` should use the include-guard form:

```c
#ifndef MODULE_NAME_H
#define MODULE_NAME_H
/* ... */
#endif // MODULE_NAME_H
```

`#pragma once` is avoided (for consistency with the existing codebase).

Headers expose only the public API surface of their module. File-scope
`static` state stays in the `.c` file. Do not put implementation details
(buffer arrays, ISR functions, FIFO structs) in headers.

## 1.5 — One translation unit = one module

Do not split a module across multiple `.c` files (e.g. `dma_audio_init.c` +
`dma_audio_isr.c`). One `.c`, one `.h`, one entry in `CMakeLists.txt`.

The exception is `.pio` files, which are paired with the module that owns
them (currently `i2s_rx_target.pio` and `i2s_rx_controller.pio` belong to
`i2s_audio`).
