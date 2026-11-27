// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __DMA_INIT_H
#define __DMA_INIT_H

// PC/AT 8237-compatible port map. The upper controller uses word spacing.
#define DMA1_MASTER_CLEAR 0x0d
#define DMA2_MASTER_CLEAR 0xda
#define DMA1_MODE 0x0b
#define DMA2_MODE 0xd6
#define DMA2_SINGLE_MASK 0xd4
#define DMA_MODE_CASCADE 0xc0

// Intel PIIX4 sections 4.2.1.2 and 4.2.1.11.
// Reset the upper controller first to close the cascade, then establish
// known modes on every channel before opening only system channel 4.
static inline void
dma_init_controllers(void)
{
    outb(0, DMA2_MASTER_CLEAR);
    outb(0, DMA1_MASTER_CLEAR);
    for (unsigned int channel = 0; channel < 4; channel++)
        outb(channel, DMA1_MODE);
    for (unsigned int channel = 1; channel < 4; channel++)
        outb(channel, DMA2_MODE);
    outb(DMA_MODE_CASCADE, DMA2_MODE);
    outb(0, DMA2_SINGLE_MASK);
}

#endif
