# Code Style & Language Rules

## 5.1 — Formatting is `clang-format`

Formatting is defined in `.clang-format` at the repo root. The relevant
settings:

- `BasedOnStyle: LLVM`
- `IndentWidth: 4`
- `TabWidth: 4`
- `UseTab: Never`
- `BreakBeforeBraces: Attach`
- `ColumnLimit: 100`
- `AllowShortIfStatementsOnASingleLine: false`
- `AllowShortFunctionsOnASingleLine: None`

All new code should conform to this style.

## 5.2 — Language standard

- C: `CMAKE_C_STANDARD 11` (set in `CMakeLists.txt`). New code should be
  valid C11.
- C++: `CMAKE_CXX_STANDARD 17` is configured for SDK compatibility, but
  this is a C project. Do not add `.cpp` files without an explicit reason.

## 5.3 — Headers

- Include guards in the form `#ifndef MODULE_H` / `#define MODULE_H` /
  `#endif // MODULE_H` (see 1.4).
- `#pragma once` is avoided.
- Keep headers minimal: declare only the public API. File-scope
  implementation details stay in `.c`.

## 5.4 — Preserve `// IWYU pragma: keep`

The pragma `// IWYU pragma: keep` is used to preserve intentionally-named
SDK headers (e.g. `pico/stdlib.h` in `main.c`, `i2s_audio.c`,
`dma_audio.c`; `tusb.h` in `usb_descriptors.c`) whose symbols are pulled
in transitively but whose explicit `#include` documents intent.

Removing these pragmas or the headers they protect should be avoided unless
the plan explicitly justifies it.

## 5.5 — Concurrency annotations

State that is written from one execution context (ISR, USB callback) and
read from another (main loop, second core) should be declared `volatile`.
Examples in the existing code:

- `buffer_a_ready`, `buffer_b_ready` in `dma_audio.c` — written in ISR,
  read in main loop.
- `overflow_count` in `usb_audio.c` — incremented inside
  `usb_audio_submit_buffer` on each dropped buffer, read by
  `usb_audio_get_overflow_count` from the main loop diagnostic printer.

`volatile` is necessary but not sufficient for ordering — combine with
`__dmb()` (DMB memory barrier) for ISR/main hand-offs. See `02-audio-pipeline.md`.

## 5.6 — Comments

- Block comments explaining hardware contracts, RP2350 workarounds, or
  TinyUSB quirks are part of the source code and should not be deleted as
  cleanup. They are the only documentation of why specific code paths
  exist.
- Trivial line comments restating what the code obviously does are
  discouraged.
- TODO/FIXME comments are acceptable when they reference a specific
  architectural limitation (see the "ARCHITECTURAL LIMITATION" comment
  in `i2s_audio.c`, documented at
  `suspected-issues.md`).

## 5.7 — Build system

`src/CMakeLists.txt` does not exist; everything is in the root
`CMakeLists.txt`. New `.c` files should be added to the `add_executable`
list. New `.pio` files should get a `pico_generate_pio_header` entry. New
SDK library dependencies go in `target_link_libraries`.

Do not introduce subdirectory `CMakeLists.txt` files without a plan that
explicitly justifies the split.
