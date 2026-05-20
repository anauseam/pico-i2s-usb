#include "usb_audio.h"
#include "tusb.h"
#include "audio_config.h"
#include "usb_descriptors.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "hardware/structs/usb.h"
#include "hardware/regs/usb.h"

void usb_audio_init(void) {
    // Initialize the TinyUSB stack
    tusb_init();
}

void usb_audio_task(void) {
    // This MUST be called frequently in the main while(1) loop
    // to process USB events and enumeration.
    tud_task();
}

void usb_audio_send_buffer(uint32_t *buffer, uint32_t size) {
    // Size is the number of 32-bit elements (256).
    // We multiply by 4 to get total bytes (1024 bytes).
    uint32_t bytes_to_send = size * 4;

    // Check if the host has opened the audio stream and is listening
    if (tud_audio_mounted()) {
        // The zero-conversion trick! We just cast the 32-bit I2S array to bytes
        // and copy it straight into the USB FIFO. The UAC2 descriptors
        // tell the PC exactly how to parse the 32-bit containers, saving DSP CPU cycles.
        // (Note: This is not '0-CPU' as it still requires a memory copy by tud_audio_write,
        // but it avoids any mathematical format conversions).
        tud_audio_write((const uint8_t *)buffer, bytes_to_send);
    }
}

// --- TINYUSB AUDIO CALLBACKS ---

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex); // Correctly gets entity ID from upper byte

    // Clock Source unit (ID = 4)
    if (entityID == 4) {
        if (ctrlSel == AUDIO_CS_CTRL_SAM_FREQ) {
            if (p_request->bRequest == AUDIO_CS_REQ_CUR) {
                static uint32_t sampFreq = SAMPLE_RATE;
                return tud_control_xfer(rhport, p_request, &sampFreq, sizeof(sampFreq));
            } else if (p_request->bRequest == AUDIO_CS_REQ_RANGE) {
                static audio_control_range_4_n_t(1) range = {
                    .wNumSubRanges = 1,
                    .subrange[0] = { .bMin = SAMPLE_RATE, .bMax = SAMPLE_RATE, .bRes = 0 }
                };
                return tud_control_xfer(rhport, p_request, &range, sizeof(range));
            }
        } else if (ctrlSel == AUDIO_CS_CTRL_CLK_VALID) {
            static audio_control_cur_1_t cur_valid = { .bCur = 1 };
            return tud_control_xfer(rhport, p_request, &cur_valid, sizeof(cur_valid));
        }
    }

    // Feature unit (ID = 2) - Dummy responses to keep host happy
    if (entityID == 2) {
        if (ctrlSel == AUDIO_FU_CTRL_MUTE && p_request->bRequest == AUDIO_CS_REQ_CUR) {
            static uint8_t mute = 0;
            return tud_control_xfer(rhport, p_request, &mute, sizeof(mute));
        } else if (ctrlSel == AUDIO_FU_CTRL_VOLUME) {
            if (p_request->bRequest == AUDIO_CS_REQ_CUR) {
                static uint16_t volume = 0;
                return tud_control_xfer(rhport, p_request, &volume, sizeof(volume));
            } else if (p_request->bRequest == AUDIO_CS_REQ_RANGE) {
                static audio_control_range_2_n_t(1) range = {
                    .wNumSubRanges = 1,
                    .subrange[0] = { .bMin = 0, .bMax = 0, .bRes = 0 }
                };
                return tud_control_xfer(rhport, p_request, &range, sizeof(range));
            }
        }
    }

    // Fallback: silently ACK unknown GET requests with 0 length.
    // UAC2 semantically expects a STALL (return false) for unsupported features.
    // However, we explicitly return 0-length data to prevent TinyUSB from issuing STALLs, 
    // which currently lock up the RP2350 USB hardware peripheral.
    return tud_control_xfer(rhport, p_request, NULL, 0);
}

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request,
                                 uint8_t *buf) {
    (void)rhport;
    (void)buf;
    // Accept all SET requests silently to prevent STALL.
    // NOTE: This intentionally swallows OS sample rate changes (SAM_FREQ) because 
    // the I2S ADC hardware is fixed at a single rate (e.g. 48kHz). This is an 
    // acceptable limitation for a fixed-rate capture device.
    return true;
}

// Override all other weak callbacks to prevent STALLs that trigger RP2350 hardware lockup
bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff) {
    (void)rhport; (void)p_request; (void)pBuff;
    return true;
}

bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff) {
    (void)rhport; (void)p_request; (void)pBuff;
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
    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport;
    (void)p_request;

    // TinyUSB on RP2040/2350 skips closing ISO endpoints because of TUP_DCD_EDPT_ISO_ALLOC.
    // If the host requests Alt 0, the hardware Buffer Control register is left with USB_BUF_CTRL_AVAIL set.
    // When the host requests Alt 1 again, TinyUSB attempts to set AVAIL and panics ("ep 81 was already available").
    // As a workaround, we manually clear the AVAIL and FULL bits for the audio endpoint.
    uint8_t ep_num = EPNUM_AUDIO_IN & 0x7F;
    uint32_t volatile *buf_ctrl = &usb_dpram->ep_buf_ctrl[ep_num].in;
    
    // Clear bits for both buffer 0 and buffer 1 (in case double buffering is ever enabled)
    uint32_t mask = USB_BUF_CTRL_AVAIL | USB_BUF_CTRL_FULL;
    *buf_ctrl &= ~(mask | (mask << 16));

    // Note: TinyUSB's internal hw_endpoint_t struct has an 'active' flag that remains stale,
    // but the IRQ handler will clear it gracefully, so this hardware workaround is sufficient.

    return true;
}