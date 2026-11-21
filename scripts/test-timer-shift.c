// SPDX-License-Identifier: LGPL-3.0-or-later
// Verify the SeaBIOS TSC projection against normal 64-bit arithmetic.
#include "hw/timer-shift.h"

int
main(void)
{
    static const struct {
        u32 low;
        u32 high;
    } inputs[] = {
        { 0, 0 },
        { 0xffffffff, 0xffffffff },
        { 0x89abcdef, 0x01234567 },
        { 0, 0x80000000 },
        { 0x80000000, 0 },
        { 1, 1 },
    };

    for (u32 i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        u64 value = ((u64)inputs[i].high << 32) | inputs[i].low;

        for (u32 shift = 0; shift < 64; shift++) {
            u32 expected = (u32)(value >> shift);

            if (timer_tsc_low32(inputs[i].low, inputs[i].high, shift)
                != expected)
                return 1;
        }
        if (timer_tsc_low32(inputs[i].low, inputs[i].high, 64) != 0
            || timer_tsc_low32(inputs[i].low, inputs[i].high, 255) != 0)
            return 2;
    }
    return 0;
}
