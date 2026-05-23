#include "usb_audio.h"
#include "audio_config.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "usb_descriptors.h"

#include "hardware/regs/usb.h"
#include "hardware/structs/usb.h"

// --- DIAGNOSTIC COUNTER ---
// Incremented when usb_audio_submit_buffer drops a DMA buffer because the
// TinyUSB ep_in_ff FIFO does not have room for the full write.
// See docs/internals/02-audio-pipeline.md
static volatile uint32_t overflow_count = 0;

void usb_audio_init(void) {
    // Initialize the TinyUSB stack
    tusb_init();
}

void usb_audio_task(void) {
    // This MUST be called frequently in the main while(1) loop
    // to process USB events and enumeration.
    tud_task();
}

void usb_audio_submit_buffer(uint32_t *buffer, uint32_t n_words) {
    // If unplugged or not yet mounted, drop silently. Not counted as overflow
    // because the cause is "no host", not "host is too slow".
    if (!tud_audio_mounted()) {
        return;
    }

    // Byte length is always a multiple of 4 by construction (one 32-bit
    // sample container per word). This is the structural defense against the
    // byte/word alignment hazard that motivated this refactor.
    // See docs/internals/02-audio-pipeline.md
    uint16_t bytes = (uint16_t)(n_words * 4);

    // Check room atomically (vs the TX side) BEFORE writing. If the full
    // buffer does not fit, drop the entire buffer; never attempt a partial
    // write. A partial write would split a 32-bit sample container across
    // two USB packets at a byte-aligned but not word-aligned boundary,
    // re-introducing exactly the bug we are fixing.
    // See docs/internals/02-audio-pipeline.md
    tu_fifo_t *ff = tud_audio_get_ep_in_ff();
    if (ff == NULL || tu_fifo_remaining(ff) < bytes) {
        overflow_count++;
        return;
    }

    uint16_t written = tud_audio_write((const uint8_t *)buffer, bytes);
    if (written != bytes) {
        // Defensive: tu_fifo_remaining() said there was room. If we reach
        // here it indicates a TinyUSB-internal inconsistency. Count it as
        // an overflow event for visibility.
        overflow_count++;
    }
}

uint32_t usb_audio_get_overflow_count(void) {
    return overflow_count;
}

// --- TINYUSB AUDIO CALLBACKS ---

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex); // Correctly gets entity ID from upper byte

    // Clock Source unit
    if (entityID == UAC2_ENTITY_CLOCK_SOURCE) {
        if (ctrlSel == AUDIO_CS_CTRL_SAM_FREQ) {
            if (p_request->bRequest == AUDIO_CS_REQ_CUR) {
                static uint32_t sampFreq = SAMPLE_RATE;
                return tud_control_xfer(rhport, p_request, &sampFreq, sizeof(sampFreq));
            } else if (p_request->bRequest == AUDIO_CS_REQ_RANGE) {
                static audio_control_range_4_n_t(1)
                    range = {.wNumSubRanges = 1,
                             .subrange[0] = {.bMin = SAMPLE_RATE, .bMax = SAMPLE_RATE, .bRes = 0}};
                return tud_control_xfer(rhport, p_request, &range, sizeof(range));
            }
        } else if (ctrlSel == AUDIO_CS_CTRL_CLK_VALID) {
            static audio_control_cur_1_t cur_valid = {.bCur = 1};
            return tud_control_xfer(rhport, p_request, &cur_valid, sizeof(cur_valid));
        }
    }

    // Feature unit - Dummy responses to keep host happy
    if (entityID == UAC2_ENTITY_FEATURE_UNIT) {
        if (ctrlSel == AUDIO_FU_CTRL_MUTE && p_request->bRequest == AUDIO_CS_REQ_CUR) {
            static uint8_t mute = 0;
            return tud_control_xfer(rhport, p_request, &mute, sizeof(mute));
        } else if (ctrlSel == AUDIO_FU_CTRL_VOLUME) {
            if (p_request->bRequest == AUDIO_CS_REQ_CUR) {
                static uint16_t volume = 0;
                return tud_control_xfer(rhport, p_request, &volume, sizeof(volume));
            } else if (p_request->bRequest == AUDIO_CS_REQ_RANGE) {
                static audio_control_range_2_n_t(1)
                    range = {.wNumSubRanges = 1, .subrange[0] = {.bMin = 0, .bMax = 0, .bRes = 0}};
                return tud_control_xfer(rhport, p_request, &range, sizeof(range));
            }
        }
    }

    // Fallback: silently ACK unknown GET requests with 0 length.
    // UAC2 semantically expects a STALL (return false) for unsupported features.
    // However, we explicitly return 0-length data to prevent TinyUSB from issuing STALLs,
    // which currently lock up the RP2350 USB hardware peripheral.
    // See docs/internals/03-usb-stack.md;
    //     docs/internals/suspected-issues.md#stall-and-rp2350-lockup
    return tud_control_xfer(rhport, p_request, NULL, 0);
}

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request,
                                 uint8_t *buf) {
    (void)rhport;
    (void)buf;
    // Accept all SET requests silently to prevent STALL.
    // NOTE: This intentionally swallows OS sample rate changes (SAM_FREQ) because
    // the I2S ADC hardware is fixed at a single rate.
    // See docs/internals/03-usb-stack.md,
    //     docs/internals/suspected-issues.md#stall-and-rp2350-lockup
    return true;
}

// Override all other weak callbacks to prevent STALLs that trigger RP2350 hardware lockup
bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request,
                             uint8_t *pBuff) {
    (void)rhport;
    (void)p_request;
    (void)pBuff;
    return true;
}

bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request,
                              uint8_t *pBuff) {
    (void)rhport;
    (void)p_request;
    (void)pBuff;
    return true;
}

bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    return tud_control_xfer(rhport, p_request, NULL, 0);
}

bool tud_audio_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    return tud_control_xfer(rhport, p_request, NULL, 0);
}

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport;
    (void)p_request;
    // No application-side stream state to reset on Alt change: the application
    // owns no FIFO and no streaming latch. TinyUSB's ep_in_ff is implicitly
    // drained by host polling; whatever stale samples remain at Alt 1 entry
    // will be flushed within a few frames at the current rate (~1500 bytes /
    // 768 bytes/frame ≈ 2 ms of pre-roll).
    // See docs/internals/02-audio-pipeline.md,
    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport;
    (void)p_request;

    // Workaround: TinyUSB on RP2040/2350 skips closing ISO endpoints because
    // of TUP_DCD_EDPT_ISO_ALLOC. If the host requests Alt 0, the hardware Buffer
    // Control register is left with USB_BUF_CTRL_AVAIL set. When the host
    // requests Alt 1 again, TinyUSB attempts to set AVAIL and panics
    // ("ep 81 was already available"). As a workaround, we manually clear the
    // AVAIL and FULL bits for the audio endpoint.
    // See docs/internals/06-workarounds.md,
    //     docs/internals/03-usb-stack.md
    uint8_t ep_num = EPNUM_AUDIO_IN & 0x7F;
    uint32_t volatile *buf_ctrl = &usb_dpram->ep_buf_ctrl[ep_num].in;

    // Clear bits for both buffer 0 and buffer 1 (in case double buffering is ever enabled)
    uint32_t mask = USB_BUF_CTRL_AVAIL | USB_BUF_CTRL_FULL;
    *buf_ctrl &= ~(mask | (mask << 16));

    // Note: TinyUSB's internal hw_endpoint_t struct has an 'active' flag that remains stale,
    // but the IRQ handler will clear it gracefully, so this hardware workaround is sufficient.

    return true;
}
