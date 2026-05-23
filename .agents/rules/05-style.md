# Code Style & Language Rules

## R5.1 — Formatting is `clang-format`

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

All new code MUST conform. Do not change `.clang-format` to accommodate a
style preference; if `clang-format` reformats your code unexpectedly, the
code is what changes, not the rules.

## R5.2 — Language standard

- C: `CMAKE_C_STANDARD 11` (set in `CMakeLists.txt`). New code MUST be
  valid C11.
- C++: `CMAKE_CXX_STANDARD 17` is configured for SDK compatibility, but
  this is a C project. Do not add `.cpp` files without an explicit reason.

## R5.3 — Headers

- Include guards in the form `#ifndef MODULE_H` / `#define MODULE_H` /
  `#endif // MODULE_H` (see R1.4).
- `#pragma once` is forbidden.
- Keep headers minimal: declare only the public API. File-scope
  implementation details stay in `.c`.

## R5.4 — Preserve `// IWYU pragma: keep`

The pragma `// IWYU pragma: keep` is used to preserve intentionally-named
SDK headers (e.g. `pico/stdlib.h` in `main.c`, `i2s_audio.c`,
`dma_audio.c`; `tusb.h` in `usb_descriptors.c`) whose symbols are pulled
in transitively but whose explicit `#include` documents intent.

Removing these pragmas or the headers they protect is a violation unless
the plan explicitly justifies it.

## R5.5 — Forbidden constructs

In production code (everything under `src/` except possibly experimental
guarded blocks):

- `goto` is forbidden.
- `setjmp` / `longjmp` are forbidden.
- Dynamic memory allocation (`malloc`/`calloc`/`realloc`/`free` and any
  C++ `new`/`delete`) is forbidden.
- Recursion deeper than 1 level is forbidden in hot-path code (see R2.2).
- Variable-length arrays (VLAs) are forbidden.

## R5.6 — Concurrency annotations

State that is written from one execution context (ISR, USB callback) and
read from another (main loop, second core) MUST be declared `volatile`.
Examples in the existing code:

- `buffer_a_ready`, `buffer_b_ready` in `dma_audio.c` — written in ISR,
  read in main loop.
- `app_fifo.head`, `app_fifo.tail`, `app_fifo.host_is_recording`,
  `app_fifo.overflow_flag`, `app_fifo.underrun_flag` in `usb_audio.c`.

`volatile` is necessary but not sufficient for ordering — combine with
`__dmb()` (DMB memory barrier) for ISR/main hand-offs and with
`__asm__ volatile("" ::: "memory")` for SPSC ring buffer head/tail
updates. See R2.4 and R2.5.

## R5.7 — Comments

- Block comments explaining hardware contracts, RP2350 workarounds, or
  TinyUSB quirks are part of the source code and MUST NOT be deleted as
  cleanup. They are the only documentation of why specific code paths
  exist.
- Trivial line comments restating what the code obviously does are
  discouraged.
- TODO/FIXME comments are acceptable when they reference a specific
  architectural limitation (see the "ARCHITECTURAL LIMITATION" comment
  in `i2s_audio.c` about PIO frame-alignment recovery).

## R5.8 — Build system

`src/CMakeLists.txt` does not exist; everything is in the root
`CMakeLists.txt`. New `.c` files MUST be added to the `add_executable`
list. New `.pio` files MUST get a `pico_generate_pio_header` entry. New
SDK library dependencies go in `target_link_libraries`.

Do not introduce subdirectory `CMakeLists.txt` files without a plan that
explicitly justifies the split.
