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
    // Pitch is a nine-bit value; the VRAM ratio scales logical bytes.
    for (int ratio = 1; ratio <= 4; ratio *= 2) {
        int max_pitch = 0x1ff * 8 / ratio;
        assert(cirrus_pitch_encode(0, ratio) == -1);
        assert(cirrus_pitch_encode(-1, ratio) == -1);
        assert(cirrus_pitch_encode(max_pitch, ratio) == 0x1ff);
        assert(cirrus_pitch_encode(max_pitch + 1, ratio) == -1);
        assert(cirrus_pitch_encode(0x100 * 8 / ratio, ratio) == 0x100);
        assert(cirrus_pitch_encode(1, ratio) == 1);

        int max_start = (0xfffff * 4 + 3) / ratio;
        assert(cirrus_start_encode(0, ratio) == 0);
        assert(cirrus_start_encode(-1, ratio) == -1);
        assert(cirrus_start_encode(max_start, ratio) == 0xfffff);
        assert(cirrus_start_encode(max_start + 1, ratio) == -1);
    }
    assert(cirrus_pitch_encode(640, 1) == 80);
    assert(cirrus_pitch_encode(1601, 1) == 201);
    assert(cirrus_pitch_encode(640, 0) == -1);
    assert(cirrus_start_encode(640, 0) == -1);
    return 0;
}
