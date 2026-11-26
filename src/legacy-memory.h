// Legacy BIOS memory-size projection from the sorted E820 map.
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __LEGACY_MEMORY_H
#define __LEGACY_MEMORY_H

#include "e820map.h"

#define LEGACY_ONE_MIB  0x100000u
#define LEGACY_16_MIB   0x1000000u
#define LEGACY_4_GIB    0x100000000ull

// Return contiguous usable bytes beginning at base, below 4 GiB.
// Stop at the first gap or non-RAM entry (including ACPI reservations).
// The caller uses a base >= 1 MiB, so the byte count fits in u32.
static inline u32
legacy_contiguous_ram(const struct e820entry *map, int count, u32 base)
{
    u64 end = base;
    int i;
    for (i = 0; i < count; i++) {
        const struct e820entry *e = &map[i];
        if (!e->size)
            continue;
        if (e->start > end)
            break;
        if (e->start >= LEGACY_4_GIB)
            break;
        u64 entry_end = e->size > LEGACY_4_GIB - e->start
            ? LEGACY_4_GIB : e->start + e->size;
        if (entry_end <= end)
            continue;
        if (e->type != E820_RAM)
            break;
        end = entry_end;
        if (end == LEGACY_4_GIB)
            break;
    }
    return end - base;
}

static inline u16
legacy_88_kib(u32 contiguous)
{
    return MIN(contiguous / 1024, 63 * 1024u);
}

static inline u16
legacy_e801_low_kib(u32 contiguous)
{
    return MIN(contiguous / 1024, 15 * 1024u);
}

static inline u16
legacy_e801_high_blocks(u32 contiguous)
{
    return MIN(contiguous / 65536, 0xffffu);
}

#endif
