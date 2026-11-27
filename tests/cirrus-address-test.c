// SPDX-License-Identifier: LGPL-3.0-or-later
#include <assert.h>
#include "../vgasrc/clext-address.h"

int main(void)
{
    for (unsigned int addr = 0; addr < 0x100000; addr++) {
        unsigned int cr1b = cirrus_start_cr1b(addr);
        unsigned int cr1d = cirrus_start_cr1d(addr);

        assert((cr1b & ~0x0dU) == 0);
        assert((cr1d & ~0x80U) == 0);
        assert(cirrus_start_decode(addr & 0xff, (addr >> 8) & 0xff,
                                   cr1b | 0xf2, cr1d | 0x7f) == addr);
    }

    // Packed-pixel byte addresses at the high-bit boundaries.
    assert(cirrus_start_cr1b(0x10000) == 0x01);
    assert(cirrus_start_cr1b(0x20000) == 0x04);
    assert(cirrus_start_cr1b(0x40000) == 0x08);
    assert(cirrus_start_cr1d(0x80000) == 0x80);
    assert(cirrus_start_cr1b(0x0800) == 0);
    assert(cirrus_start_cr1d(0x0800) == 0);
    return 0;
}
