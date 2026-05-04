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

#endif // DMA_AUDIO_H