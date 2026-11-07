#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#include "audio_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- System / Core ---
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE
#define CFG_TUSB_OS OPT_OS_PICO
#ifdef CFG_TUSB_DEBUG
#undef CFG_TUSB_DEBUG
#endif
#define CFG_TUSB_DEBUG 0 // Disable debug logging on UART to prevent audio bottleneck
#define CFG_TUD_ENDPOINT0_SIZE 64

// --- Enabled USB Classes ---
#define CFG_TUD_AUDIO 1

// --- Audio Class Configuration ---
// Descriptor length for our 2-channel microphone Audio Function (includes IAD).
#define TUD_AUDIO_MIC_TWO_CH_DESC_LEN                                                              \
    (TUD_AUDIO_DESC_IAD_LEN + TUD_AUDIO_DESC_STD_AC_LEN + TUD_AUDIO_DESC_CS_AC_LEN +               \
     TUD_AUDIO_DESC_CLK_SRC_LEN + TUD_AUDIO_DESC_INPUT_TERM_LEN + TUD_AUDIO_DESC_OUTPUT_TERM_LEN + \
     TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL_LEN + TUD_AUDIO_DESC_STD_AS_INT_LEN +                 \
     TUD_AUDIO_DESC_STD_AS_INT_LEN + TUD_AUDIO_DESC_CS_AS_INT_LEN +                                \
     TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN +                         \
     TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN)

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN TUD_AUDIO_MIC_TWO_CH_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT 1 // 1 Audio Streaming Interface
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ 64

#define CFG_TUD_AUDIO_ENABLE_EP_IN 1

// Disable flow control — it requires sample_rate_tx to be set via SET_CUR
// before SET_INTERFACE, but Linux sends SET_INTERFACE first. With flow
// control enabled, audiod_calc_tx_packet_sz() silently fails and leaves
// packet_sz_tx at zero, making the TX path fragile.
#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL 0

#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX 4 // 32-bit container

// Isochronous Endpoint settings
// In bytes. A USB frame lasts 1 ms; the extra audio frame (+ 1) carries 44.1 kHz's
// 45-frame packets and lets the device catch up when the ADC runs slightly fast.
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX                                                          \
    (((AUDIO_RATE_MAX / 1000) + 1) * AUDIO_CHANNELS * CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX)
// TinyUSB's FIFO between the DMA buffers and the USB packets (ep_in_ff), in bytes.
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ 4096

// The two-packet margin lets the host fall at least 1 ms behind before a whole DMA
// buffer is dropped.
#if AUDIO_BUFFER_SIZE * CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX +                               \
        2 * CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX >                                                    \
    CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ
#error "AUDIO_BUFFER_FRAMES is too large for the USB FIFO"
#endif

#ifdef __cplusplus
}
#endif

#endif // _TUSB_CONFIG_H_