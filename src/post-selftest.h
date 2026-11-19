#ifndef __WHP_POST_SELFTEST_H
#define __WHP_POST_SELFTEST_H

// Pure POST checks shared with the host-side regression test.  Neither
// helper allocates memory, reads outside its inputs, or accesses I/O ports.
#include "e820map.h" // struct e820entry, E820_RAM, E820_RESERVED

static inline u32
whp_post_ram_pattern(u32 index)
{
    return 0xa5a5a5a5U ^ ((index + 1U) * 0x9e3779b9U);
}

static inline void
whp_post_ram_fill(volatile u32 *words, u32 count, int inverted)
{
    u32 i;
    for (i = 0; i < count; i++) {
        u32 pattern = whp_post_ram_pattern(i);
        words[i] = inverted ? ~pattern : pattern;
    }
}

static inline int
whp_post_ram_verify(volatile u32 *words, u32 count, int inverted)
{
    u32 i;
    for (i = 0; i < count; i++) {
        u32 pattern = whp_post_ram_pattern(i);
        if (words[i] != (inverted ? ~pattern : pattern))
            return -1;
    }
    return 0;
}

static inline int
whp_post_ram_walk(volatile u32 *word)
{
    u32 bit;
    for (bit = 0; bit < 32; bit++) {
        u32 pattern = 1U << bit;
        *word = pattern;
        if (*word != pattern)
            return -1;
        *word = ~pattern;
        if (*word != ~pattern)
            return -1;
    }
    return 0;
}

// Check the finalized map's ordering, nonzero lengths and overflow before
// trusting a QEMU-provided map.  Gaps are legal, overlaps are not.
static inline int
whp_post_e820_valid(const struct e820entry *entries, u32 count)
{
    u64 previous_end = 0;
    int seen_ram = 0;
    u32 i;
    if (!count)
        return -1;
    for (i = 0; i < count; i++) {
        const struct e820entry *entry = &entries[i];
        u64 end = entry->start + entry->size;
        if (!entry->size || end < entry->start ||
            (i && entry->start < previous_end))
            return -1;
        if (entry->type == E820_RAM)
            seen_ram = 1;
        previous_end = end;
    }
    return seen_ram ? 0 : -1;
}

static inline int
whp_post_e820_reserves(const struct e820entry *entries, u32 count,
                       u64 start, u64 size)
{
    u32 i;
    if (!size || start + size < start)
        return 0;
    for (i = 0; i < count; i++) {
        const struct e820entry *entry = &entries[i];
        if (entry->type == E820_RESERVED &&
            entry->start <= start &&
            entry->size >= size &&
            entry->start + entry->size >= entry->start &&
            entry->start + entry->size >= start + size)
            return 1;
    }
    return 0;
}

#endif // __WHP_POST_SELFTEST_H
