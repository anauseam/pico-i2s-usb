# Suspected Issues — Descriptive Context

This file documents **suspected-but-unreproduced** hazards in the codebase
and the defensive code that addresses them. Unlike the docs in `docs/internals/`, this
file is mainly descriptive and provides historical context for certain workarounds.

Each entry describes one concern: where the defensive code lives, what we
suspect, why we suspect it, the current reproduction status, and what to
do if a future maintainer demonstrates the suspicion is unfounded.

Docs in `docs/internals/` may cite entries here as background. The constraint
itself stands on its own — refuting an entry here does NOT automatically
invalidate a constraint that cross-references it. See `03-usb-stack.md`
for an example: 3.4 ("never STALL") is binding regardless of whether the
underlying RP2350 lockup is ever reproduced.

---

## `CFG_TUSB_DEBUG` and audio stream rate {#cfg-tusb-debug-and-audio-stream-rate}

**Location:** `src/tusb_config.h` (the `#undef`/`#define CFG_TUSB_DEBUG 0` block).

**Code excerpt:**

```c
#ifdef CFG_TUSB_DEBUG
#undef CFG_TUSB_DEBUG
#endif
#define CFG_TUSB_DEBUG 0
```

**What we suspect:** Enabling `CFG_TUSB_DEBUG >= 1` causes TinyUSB to emit
per-transfer log output over the UART. The cost of those `printf`s blocks
`tud_task()` for long enough to starve the audio IN stream — the host
observes audio arriving at a fraction of its expected rate (e.g. visible
in Audacity as the recording timeline advancing slower than wall-clock).

**Why we suspect it:** The performance characteristics of `printf` over
UART on the RP2350 (115200 baud, single-buffered) make blocking-time-per-
transfer arithmetic plausible at the per-IN-microframe rate. The explicit
`#undef` first defends against the SDK or build system pre-defining a
non-zero value through compiler flags.

**Reproduction status:** Unverified. Logged here for institutional memory.

**Action if refuted:** If a future maintainer demonstrates that
`CFG_TUSB_DEBUG=1` does *not* perturb the audio stream rate on a
representative host, the `#undef`/`#define` block can be removed and
`CFG_TUSB_DEBUG` can be left to whatever the SDK or build system sets.
Until then the block stays in `tusb_config.h`.

---

## `tu_static` alignment on RISC-V {#tu-static-alignment-on-riscv}

**Location:** `src/tusb_config.h` (the `#undef tu_static` block).

**Code excerpt:**

```c
#undef tu_static
#define tu_static static __attribute__((aligned(4)))
```

**What we suspect:** TinyUSB declares several internal byte arrays (e.g.
`ctrl_buf_1`) via `tu_static`. The default GCC alignment for `uint8_t[]`
is 1-byte. On the RP2350's RISC-V cores, `tu_memcpy_s` is implemented as
32-bit load/store instructions during UAC2 control requests (such as the
host's `SET_CUR(SAM_FREQ)`). Unaligned 32-bit accesses on RISC-V can
trap depending on the core's misaligned-access support. Forcing 4-byte
alignment on every `tu_static` declaration sidesteps the trap.

**Why we suspect it:** Plausible from RISC-V architectural conventions
and inspection of the `tu_memcpy_s` implementation. No concrete crash
captured in the commit history. The workaround was added defensively.

**Reproduction status:** Unverified. The codebase has run on the RP2350
RISC-V cores without crashes since the override was added; we cannot
distinguish "the override is necessary" from "we got lucky with the
specific bytes being copied happening to be naturally aligned."

**Action if refuted:** If a future maintainer can demonstrate that the
default GCC alignment is sufficient on the RP2350 (for instance by
removing the override and running an extended UAC2 control-request
fuzz against the device), the `#undef`/`#define tu_static` block can
be removed.

---

## STALL responses and RP2350 USB lockup {#stall-and-rp2350-lockup}

**Location:** `src/usb_audio.c`, all `tud_audio_*_req_*_cb` functions.

**Code excerpt:**

```c
// In tud_audio_get_req_entity_cb:
return tud_control_xfer(rhport, p_request, NULL, 0);  // zero-length ACK

// In tud_audio_set_req_entity_cb (also tud_audio_set_req_ep_cb,
// tud_audio_set_req_itf_cb):
return true;  // silent ACK
```

**What we suspect:** Returning `false` from a UAC2 control callback
causes TinyUSB to issue a STALL on endpoint 0. STALL responses on the
RP2350 USB peripheral have been associated with the device entering a
state from which it cannot recover (no further control transfers
succeed; only a power-cycle restores function).

**Why we suspect it:** Early development was punctuated by hangs that
coincided with specific UAC2 host requests; converting all callbacks
to return ACK rather than `false` made the hangs go away. The exact
causal chain is not confirmed.

**Why the rule stands regardless:** The semantically-correct UAC2
behavior for unsupported features is STALL, but a zero-length ACK is
also acceptable to all tested hosts (Linux, macOS reportedly, Windows 11
reportedly). The cost of the workaround is "the device claims to
support features it doesn't actually implement"; the benefit (if the
suspicion is real) is "no lockup." We accept the trade. This is why
`03-usb-stack.md` says to avoid STALL regardless of whether
the suspected lockup is ever reproduced.

**Reproduction status:** Suspected by historical association during
development. No clean repro in the current codebase.

**Action if refuted:** If a future maintainer can demonstrate that
returning `false` from a control callback does not lock up the RP2350
USB peripheral, 3.4 can be relaxed and unsupported features can
return `false` (the semantically-correct UAC2 behavior). Until then,
all callbacks ACK.

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

---

## Process note: clean-build verification {#clean-build-verification}

**Discovered during the v0.1.0 release cleanup commit.** A clean build of
the codebase failed because UAC2 entity-ID macros (`UAC2_ENTITY_*`) were
referenced in `src/usb_audio.c` but never defined — the constraint in
`03-usb-stack.md` was silently violated. The incremental
build had been silently passing because `usb_audio.c` had a stale `.o`.

**Future CI scope:** When CI is added (deferred to post-v0.1.0), the
pipeline MUST do a clean build (`cmake --build build --clean-first` or
equivalent) to catch this class of incremental-build dependency drift.

Not a rule yet because there is no CI. Documented here to be remembered
when CI is set up.
