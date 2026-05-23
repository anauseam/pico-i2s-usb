# pico-i2s-usb — Project Rules Overview

These docs govern the internal architecture of the `pico-i2s-usb` codebase.
They outline the technical constraints and invariants that keep the real-time audio pipeline stable.

## What this project is

Bare-metal C firmware that turns a Raspberry Pi Pico 2 (RP2350) plus an
external 24-bit I2S ADC into a driverless USB Audio Class 2.0 (UAC2)
microphone. Built on the Raspberry Pi Pico SDK and TinyUSB.

## The data pipeline

```text
ADC ──I2S──► PIO state machine (i2s_audio.c)
                │
                ▼
            DMA ping-pong (dma_audio.c)
                │  buffer_*_ready flags (volatile + __dmb)
                ▼
            main.c conductor loop
                │  usb_audio_submit_buffer →
                │  tud_audio_write(buf, AUDIO_BUFFER_SIZE * 4)
                ▼
            TinyUSB ep_in_ff ──ISO IN──► Host
                              (packet size varies per frame;
                              this IS the UAC2 async rate signal)
```

`src/main.c` is the conductor: it initializes each module, starts the pipeline,
and in its main loop polls the DMA buffer manager and forwards completed
buffers to the USB module. Nothing else orchestrates these modules.

## Where these docs live

```text
docs/
└── internals/
    ├── 00-overview.md         (this file)
    ├── 01-architecture.md     module boundaries and ownership
    ├── 02-audio-pipeline.md   real-time hot path constraints
    ├── 03-usb-stack.md        TinyUSB / UAC2 contract
    ├── 04-configuration.md    centralization of tunables
    ├── 05-style.md            code style and language rules
    ├── 06-workarounds.md      reproduced workarounds that should stay
    └── suspected-issues.md    descriptive notes on unreproduced defensive code
```

Each architecture file is self-contained. `suspected-issues.md` is descriptive only and provides historical context for certain workarounds.

## Source-of-truth file map

| Concern                             | File(s)                                              |
| ----------------------------------- | ---------------------------------------------------- |
| Entry point / conductor loop        | `src/main.c`                                         |
| All user-tunable configuration      | `src/audio_config.h`                                 |
| PIO + clock setup                   | `src/i2s_audio.{c,h}`                                |
| PIO programs                        | `src/i2s_rx_target.pio`, `src/i2s_rx_controller.pio` |
| DMA ping-pong + ISR                 | `src/dma_audio.{c,h}`                                |
| TinyUSB callbacks + ep_in_ff submit | `src/usb_audio.{c,h}`                                |
| USB descriptors + shared USB IDs    | `src/usb_descriptors.{c,h}`                          |
| TinyUSB stack config                | `src/tusb_config.h`                                  |
| Build                               | `CMakeLists.txt`, `pico_sdk_import.cmake`            |
| Format                              | `.clang-format`                                      |

## Core invariants (cross-cutting)

These principles are repeated in the more specific doc files but are listed here for a quick glance:

1. `src/main.c` is the only file allowed to call into more than one of the
   audio/USB modules.
2. The audio hot path is non-blocking. No heap allocation, no `sleep_ms`, no
   unconditional `printf`.
3. All user-tunable configuration lives in `src/audio_config.h`.
4. The Pico is ALWAYS the I2S Target. The ADC is ALWAYS the I2S Controller.
   Pico-as-Controller is empirically known to fail on this hardware and the
   supporting code paths have been removed.
5. Every workaround in `06-workarounds.md` (reproduced) and
   every defensive block referenced from `suspected-issues.md`
   (unreproduced) should stay with its block comment intact, unless a commit
   message explains the upstream fix or refutes the underlying suspicion.
