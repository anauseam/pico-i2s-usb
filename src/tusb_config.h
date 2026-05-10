#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// --- System / Core ---
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE
#define CFG_TUSB_OS OPT_OS_PICO
#define CFG_TUD_ENDPOINT0_SIZE 64

// --- Enabled USB Classes ---
#define CFG_TUD_AUDIO 1

// --- Audio Class Configuration ---
// Descriptor length for our 2-channel microphone Audio Function (includes IAD).
#define TUD_AUDIO_MIC_TWO_CH_DESC_LEN (TUD_AUDIO_DESC_IAD_LEN\
  + TUD_AUDIO_DESC_STD_AC_LEN\
  + TUD_AUDIO_DESC_CS_AC_LEN\
  + TUD_AUDIO_DESC_CLK_SRC_LEN\
  + TUD_AUDIO_DESC_INPUT_TERM_LEN\
  + TUD_AUDIO_DESC_OUTPUT_TERM_LEN\
  + TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL_LEN\
  + TUD_AUDIO_DESC_STD_AS_INT_LEN\
  + TUD_AUDIO_DESC_STD_AS_INT_LEN\
  + TUD_AUDIO_DESC_CS_AS_INT_LEN\
  + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN\
  + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN\
  + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN)

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN TUD_AUDIO_MIC_TWO_CH_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT 1   // 1 Audio Streaming Interface
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ 64

// TX FIFO: Must be large enough to hold our DMA buffer.
// 128 stereo samples * 4 bytes = 1024 bytes. 2048 gives us breathing room!
#define CFG_TUD_AUDIO_ENABLE_EP_IN 1
#define CFG_TUD_AUDIO_FUNC_1_TX_FIFO_SZ 2048
#define CFG_TUD_AUDIO_FUNC_1_RX_FIFO_SZ 0

#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX  4   // 32-bit container
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX           2   // Stereo

#include "audio_config.h"

// Isochronous Endpoint settings
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX (((SAMPLE_RATE / 1000) + ((SAMPLE_RATE % 1000) ? 1 : 0)) * 2 * 4)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ 2048

#ifdef __cplusplus
}
#endif

#endif // _TUSB_CONFIG_H_