# Suspected Issues — Descriptive Context

This file documents **suspected-but-unreproduced** hazards in the codebase
and the defensive code that addresses them. Unlike the docs in `docs/internals/`, this
file is mainly descriptive and provides historical context for certain workarounds.

Each entry describes one concern: where the defensive code lives, what we
suspect, why we suspect it, the current reproduction status, and what to
do if a future maintainer demonstrates the suspicion is unfounded.

Docs in `docs/internals/` may cite entries here as background. The constraint
itself stands on its own — refuting an entry here does NOT automatically
invalidate a constraint that cross-references it.

---

## PIO frame-misalignment recovery {#pio-frame-misalignment-recovery}

**Location:** `src/i2s_audio.c`, the `i2s_audio_init` target-mode body
(the "ARCHITECTURAL LIMITATION" comment block).

**What this documents:** The PIO target-mode program (`i2s_rx_target.pio`)
performs an LRCK-edge sync sequence on init to align its 32-bit shift
register with the ADC's I2S frame boundaries. After init, the program
runs in steady-state with no further alignment checks. If a BCLK or LRCK
glitch occurs during streaming (e.g. a brief power-supply dropout to the
ADC), the state machine may end up offset by 1–31 bits, producing
permanently corrupted samples until the device is power-cycled.

**What we know:** No glitch-recovery mechanism exists. This is a known
architectural gap, not a defensive workaround.

**Reproduction status:** This is not a suspected hazard; it is a
documented unimplemented feature. Inclusion here is for visibility,
not as a workaround that might be refuted.

**Implementing recovery would require:**

1. A watchdog timer that checks PIO RX FIFO progress.
2. A re-sync routine that disables/resets the SM and re-runs the initial
   LRCK-edge sync sequence (see the labels `wait_lrck_high`,
   `wait_lrck_low`, `wait_bclk_high_sync`, `wait_bclk_low_sync` in
   `i2s_rx_target.pio`).

Implementing the above would also require updating the
"ARCHITECTURAL LIMITATION" comment in `i2s_audio.c` and removing this
entry from this file.
