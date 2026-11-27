// Checked legacy memory reservation arithmetic.
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __UMB_ALLOC_H
#define __UMB_ALLOC_H

#include "types.h"

// Return zero on failure. Option ROM addresses are always nonzero.
static inline u32
umb_rom_end(u32 start, u32 limit, u32 size, u32 align)
{
    if (!align || (align & (align - 1)) || start > limit ||
        size > limit - start)
        return 0;
    u32 end = start + size;
    u32 padding = -end & (align - 1);
    if (padding > limit - end)
        return 0;
    return end + padding;
}

// PMM lengths are paragraphs; retain the existing signed byte-size limit.
static inline u32
pmm_paragraph_bytes(u32 paragraphs)
{
    if (paragraphs > 0x7fffffffu / 16)
        return 0;
    return paragraphs * 16;
}

#endif
