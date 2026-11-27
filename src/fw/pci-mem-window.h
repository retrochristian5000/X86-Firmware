// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __PCI_MEM_WINDOW_H
#define __PCI_MEM_WINDOW_H

#include "e820map.h"

// Ordinary PCI BARs do not belong in the 24-bit legacy ISA address range.
#define PCI_MEM32_MIN 0x01000000ULL

struct pci_mem_window {
    u64 start, end;
    u64 lower_base, upper_base;
};

// Both bounds are exclusive at the top. Check subtraction before aligning.
static inline int
pci_mem_pack(u64 lo, u64 hi, u64 lower_size, u64 lower_align,
             u64 upper_size, u64 upper_align, struct pci_mem_window *out)
{
    if (lo >= hi || !lower_align || (lower_align & (lower_align - 1))
        || !upper_align || (upper_align & (upper_align - 1))
        || upper_size > hi - lo)
        return -1;
    u64 upper = (hi - upper_size) & ~(upper_align - 1);
    if (upper < lo || lower_size > upper - lo)
        return -1;
    u64 lower = (upper - lower_size) & ~(lower_align - 1);
    if (lower < lo)
        return -1;
    *out = (struct pci_mem_window){lo, hi, lower, upper};
    return 0;
}

// E820 is sorted by address. Select the largest contiguous unused gap
// that fits both PCI resource classes; prefer the higher gap on a tie.
// Do not advertise an aperture containing RAM or a reserved ISA/MMIO range.
static inline int
pci_mem32_window(const struct e820entry *map, int count, u64 lo, u64 hi,
                 u64 lower_size, u64 lower_align, u64 upper_size,
                 u64 upper_align, struct pci_mem_window *out)
{
    if (lo < PCI_MEM32_MIN)
        lo = PCI_MEM32_MIN;
    if (hi > 0x100000000ULL || lo >= hi || count < 0)
        return -1;

    // Walk backwards once, validating all nonempty entries even outside
    // the candidate aperture. The final iteration visits the bottom gap.
    u64 best = 0, end = hi, previous_start = ~0ULL;
    for (int i = count; i >= 0; ) {
        u64 start = 0, used_end = lo;
        if (i) {
            --i;
            start = map[i].start;
            u64 size = map[i].size;
            if (!size)
                continue;
            if (start > previous_start || size > previous_start - start)
                return -1;
            previous_start = start;
            used_end = start + size;
            if (start >= end)
                continue;
            if (used_end < lo)
                used_end = lo;
        } else {
            i = -1;
        }
        if (used_end < end && end - used_end > best
            && !pci_mem_pack(used_end, end, lower_size, lower_align,
                             upper_size, upper_align, out))
            best = end - used_end;
        end = start;
    }
    return best ? 0 : -1;
}

#endif
