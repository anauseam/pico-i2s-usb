#include "usb_audio.h"
#include "audio_config.h"
#include "tusb.h"
#include "usb_descriptors.h"

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

bool usb_audio_event_pending(void) {
    return tud_task_event_ready();
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
        if (ctrlSel == AUDIO20_CS_CTRL_SAM_FREQ) {
            if (p_request->bRequest == AUDIO20_CS_REQ_CUR) {
                // TinyUSB's flow control takes the stream's sample rate from this reply:
                // Linux never sends SET_CUR to a read-only clock.
                static uint32_t sampFreq = SAMPLE_RATE;
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &sampFreq,
                                                                  sizeof(sampFreq));
            } else if (p_request->bRequest == AUDIO20_CS_REQ_RANGE) {
                static audio20_control_range_4_n_t(1)
                    range = {.wNumSubRanges = 1,
                             .subrange[0] = {.bMin = SAMPLE_RATE, .bMax = SAMPLE_RATE, .bRes = 0}};
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &range,
                                                                  sizeof(range));
            }
        } else if (ctrlSel == AUDIO20_CS_CTRL_CLK_VALID) {
            static audio20_control_cur_1_t cur_valid = {.bCur = 1};
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &cur_valid,
                                                              sizeof(cur_valid));
        }
    }

    // Feature unit - Dummy responses to keep host happy
    if (entityID == UAC2_ENTITY_FEATURE_UNIT) {
        if (ctrlSel == AUDIO20_FU_CTRL_MUTE && p_request->bRequest == AUDIO20_CS_REQ_CUR) {
            static uint8_t mute = 0;
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &mute,
                                                              sizeof(mute));
        } else if (ctrlSel == AUDIO20_FU_CTRL_VOLUME) {
            if (p_request->bRequest == AUDIO20_CS_REQ_CUR) {
                static uint16_t volume = 0;
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &volume,
                                                                  sizeof(volume));
            } else if (p_request->bRequest == AUDIO20_CS_REQ_RANGE) {
                static audio20_control_range_2_n_t(1)
                    range = {.wNumSubRanges = 1, .subrange[0] = {.bMin = 0, .bMax = 0, .bRes = 0}};
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &range,
                                                                  sizeof(range));
            }
        }
    }

    // Fallback: STALL unknown GET requests.
    // UAC2 semantically expects a STALL (return false) for unsupported features.
    return false;
}

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request,
                                 uint8_t *buf) {
    (void)rhport;
    (void)p_request;
    (void)buf;
    // STALL all unsupported SET requests.
    // NOTE: This correctly rejects OS sample rate changes (SAM_FREQ) because
    // the I2S ADC hardware is fixed at a single rate.
    return false;
}

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport;
    // TinyUSB empties ep_in_ff only when the stream stops, and usb_audio_submit_buffer keeps
    // writing while it is stopped, so the FIFO would start the stream with old audio.
    if (TU_U16_LOW(p_request->wValue) != 0) {
        tud_audio_clear_ep_in_ff();
    }
    return true;
}
