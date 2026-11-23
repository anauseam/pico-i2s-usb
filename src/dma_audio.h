#ifndef DMA_AUDIO_H
#define DMA_AUDIO_H

#include "hardware/pio.h"
#include <stdbool.h>
#include <stdint.h>

// Configures the DMA Ping-Pong chains and hardware interrupts
void dma_audio_init(PIO pio, uint sm);

// Starts the DMA hardware engine
void dma_audio_start(void);

// Checks if a ping-pong buffer has finished filling.
// If true, populates `ready_buffer` with the pointer to the fresh audio array.
bool dma_audio_get_ready_buffer(uint32_t **ready_buffer);

// True while a filled buffer waits for dma_audio_get_ready_buffer().
bool dma_audio_buffer_pending(void);

// Running count of filled buffers dropped because the main loop took neither
// before the next one filled. Diagnostic only; used under AUDIO_DEBUG_LOGGING.
uint32_t dma_audio_get_overrun_count(void);

#endif // DMA_AUDIO_H