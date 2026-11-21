// SPDX-License-Identifier: LGPL-3.0-or-later
// Extract the low 32 bits of a shifted TSC without compiler-rt helpers.
#ifndef __HW_TIMER_SHIFT_H
#define __HW_TIMER_SHIFT_H

#include "types.h"

static inline u32
timer_tsc_low32(u32 low, u32 high, u8 shift)
{
    if (shift == 0)
        return low;
    if (shift < 32)
        return (low >> shift) | (high << (32 - shift));
    if (shift < 64)
        return high >> (shift - 32);
    return 0;
}

#endif // __HW_TIMER_SHIFT_H
