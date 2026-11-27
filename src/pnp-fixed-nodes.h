// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __PNP_FIXED_NODES_H
#define __PNP_FIXED_NODES_H

// fw_cfg: one byte, bits 0-3 correspond to handles 2-5.
#define PNP_CORE_FILE "etc/isa-pnp-core-v1"
#define PNP_CORE_BITS 0x0f
#define PNP_HANDLE_LIMIT 6

#define PNP_IO(base, len) \
    0x47, 1, (base) & 0xff, (base) >> 8, \
    (base) & 0xff, (base) >> 8, 1, (len)
#define PNP_IRQ(irq) 0x22, (1U << (irq)) & 0xff, (1U << (irq)) >> 8
#define PNP_END_BLOCKS 0x79, 0, 0x79, 0, 0x79, 0
#define PNP_SYSTEM_NODE(size, handle, idhi, idlo, subtype) \
    (size), 0, (handle), 0x41, 0xd0, (idhi), (idlo), \
    0x08, (subtype), 0x01, 0x03, 0

// PNP0000: two cascaded ISA PICs and their ELCRs.
u8 PnpPicNode[] PNP_NODE_STORAGE = {
    PNP_SYSTEM_NODE(45, 2, 0x00, 0x00, 0x00),
    PNP_IO(0x20, 2), PNP_IO(0xa0, 2), PNP_IO(0x4d0, 2),
    PNP_IRQ(2), PNP_END_BLOCKS,
};

// PNP0200: PC/AT DMA controllers and the seven page registers.
// DMA channel 4 is the cascade; peripheral channels are not reserved.
u8 PnpDmaNode[] PNP_NODE_STORAGE = {
    PNP_SYSTEM_NODE(69, 3, 0x02, 0x00, 0x01),
    PNP_IO(0x00, 16), PNP_IO(0x81, 3), PNP_IO(0x87, 1),
    PNP_IO(0x89, 3), PNP_IO(0x8f, 1), PNP_IO(0xc0, 32),
    0x2a, 0x10, 0x02, PNP_END_BLOCKS,
};

u8 PnpPitNode[] PNP_NODE_STORAGE = {
    PNP_SYSTEM_NODE(29, 4, 0x01, 0x00, 0x02),
    PNP_IO(0x40, 4), PNP_IRQ(0), PNP_END_BLOCKS,
};

u8 PnpRtcNode[] PNP_NODE_STORAGE = {
    PNP_SYSTEM_NODE(29, 5, 0x0b, 0x00, 0x03),
    PNP_IO(0x70, 2), PNP_IRQ(8), PNP_END_BLOCKS,
};

static inline unsigned int
pnp_core_node_mask(unsigned long long reported)
{
    return 3U | (reported <= PNP_CORE_BITS ? reported << 2 : 0);
}

static inline unsigned int
pnp_next_node(unsigned int mask, unsigned int handle)
{
    while (++handle < PNP_HANDLE_LIMIT)
        if (mask & (1U << handle))
            return handle;
    return 0xff;
}

static inline unsigned int
pnp_core_node_size(unsigned int handle)
{
    switch (handle) {
    case 0: return 26;
    case 1: return 18;
    case 2: return sizeof(PnpPicNode);
    case 3: return sizeof(PnpDmaNode);
    case 4: return sizeof(PnpPitNode);
    case 5: return sizeof(PnpRtcNode);
    default: return 0;
    }
}

#endif
