#ifndef AUDIO_CONFIG_DERIVED_H
#define AUDIO_CONFIG_DERIVED_H

// Values computed from the settings in audio_config.h. They are not settings:
// change the settings instead. Don't include this file directly; include
// audio_config.h.

// 32-bit words per DMA buffer, one per sample.
#define AUDIO_BUFFER_SIZE (AUDIO_BUFFER_FRAMES * AUDIO_CHANNELS)

#define AUDIO_RATE_COUNT                                                                           \
    ((((AUDIO_RATES) & AUDIO_RATE_44100) != 0) + (((AUDIO_RATES) & AUDIO_RATE_48000) != 0) +       \
     (((AUDIO_RATES) & AUDIO_RATE_96000) != 0))

// The highest listed rate in Hz, or 0 if none.
#define AUDIO_RATE_MAX                                                                             \
    (((AUDIO_RATES) & AUDIO_RATE_96000)   ? 96000                                                  \
     : ((AUDIO_RATES) & AUDIO_RATE_48000) ? 48000                                                  \
     : ((AUDIO_RATES) & AUDIO_RATE_44100) ? 44100                                                  \
                                          : 0)

// The lowest listed rate in Hz, or 0 if none: the rate the device reports to the
// host and expects from the ADC.
#define SAMPLE_RATE                                                                                \
    (((AUDIO_RATES) & AUDIO_RATE_44100)   ? 44100                                                  \
     : ((AUDIO_RATES) & AUDIO_RATE_48000) ? 48000                                                  \
     : ((AUDIO_RATES) & AUDIO_RATE_96000) ? 96000                                                  \
                                          : 0)

#endif // AUDIO_CONFIG_DERIVED_H
