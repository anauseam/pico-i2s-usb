#include "dma_audio.h"
#include "audio_config.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include <stddef.h>

// Raw Buffer Memory
static uint32_t audio_buffer_a[AUDIO_BUFFER_SIZE];
static uint32_t audio_buffer_b[AUDIO_BUFFER_SIZE];

// Volatile State Flags
static volatile bool buffer_a_ready = false;
static volatile bool buffer_b_ready = false;

// DMA Hardware Channels
static int dma_chan_a;
static int dma_chan_b;

// ISR Handler
static void dma_handler() {
    if (dma_hw->ints0 & (1u << dma_chan_a)) {
        dma_hw->ints0 = 1u << dma_chan_a;
        dma_channel_set_trans_count(dma_chan_a, AUDIO_BUFFER_SIZE, false);
        dma_channel_set_write_addr(dma_chan_a, audio_buffer_a, false);

        __dmb();
        buffer_a_ready = true;
    }

    if (dma_hw->ints0 & (1u << dma_chan_b)) {
        dma_hw->ints0 = 1u << dma_chan_b;
        dma_channel_set_trans_count(dma_chan_b, AUDIO_BUFFER_SIZE, false);
        dma_channel_set_write_addr(dma_chan_b, audio_buffer_b, false);

        __dmb();
        buffer_b_ready = true;
    }
}

void dma_audio_init(PIO pio, uint sm) {
    dma_chan_a = dma_claim_unused_channel(true);
    dma_chan_b = dma_claim_unused_channel(true);

    dma_channel_config dma_config_a = dma_channel_get_default_config(dma_chan_a);
    channel_config_set_transfer_data_size(&dma_config_a, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config_a, false);
    channel_config_set_write_increment(&dma_config_a, true);
    channel_config_set_dreq(&dma_config_a, pio_get_dreq(pio, sm, false));
    channel_config_set_chain_to(&dma_config_a, dma_chan_b);

    dma_channel_config dma_config_b = dma_channel_get_default_config(dma_chan_b);
    channel_config_set_transfer_data_size(&dma_config_b, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config_b, false);
    channel_config_set_write_increment(&dma_config_b, true);
    channel_config_set_dreq(&dma_config_b, pio_get_dreq(pio, sm, false));
    channel_config_set_chain_to(&dma_config_b, dma_chan_a);

    dma_channel_set_irq0_enabled(dma_chan_a, true);
    dma_channel_set_irq0_enabled(dma_chan_b, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_configure(dma_chan_a, &dma_config_a, audio_buffer_a, &pio->rxf[sm],
                          AUDIO_BUFFER_SIZE, false);
    dma_channel_configure(dma_chan_b, &dma_config_b, audio_buffer_b, &pio->rxf[sm],
                          AUDIO_BUFFER_SIZE, false);
}

void dma_audio_start(void) {
    dma_channel_start(dma_chan_a);
}

bool dma_audio_get_ready_buffer(uint32_t **ready_buffer) {
    if (buffer_a_ready) {
        __dmb();
        buffer_a_ready = false;
        *ready_buffer = audio_buffer_a;
        return true;
    }
    if (buffer_b_ready) {
        __dmb();
        buffer_b_ready = false;
        *ready_buffer = audio_buffer_b;
        return true;
    }
    return false;
}