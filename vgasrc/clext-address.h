// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef CLEXT_ADDRESS_H
#define CLEXT_ADDRESS_H

// CL-GD5446 TRM 8.32 and 8.34: Screen Start A is a 20-bit address.
static inline unsigned int
cirrus_start_decode(unsigned int lo, unsigned int hi,
                    unsigned int cr1b, unsigned int cr1d)
{
    return lo | (hi << 8) | ((cr1b & 1) << 16)
        | ((cr1b & 0x0c) << 15) | ((cr1d & 0x80) << 12);
}

static inline unsigned int
cirrus_start_cr1b(unsigned int addr)
{
    return ((addr & 0x10000) >> 16) | ((addr & 0x60000) >> 15);
}

static inline unsigned int
cirrus_start_cr1d(unsigned int addr)
{
    return (addr & 0x80000) >> 12;
}

// CL-GD5446 TRM 4.40: CR13 and CR1B[4] encode a nine-bit pitch.
// Check the byte request before scaling so it cannot wrap the register.
static inline int
cirrus_pitch_encode(int bytes, int ratio)
{
    if (bytes <= 0 || ratio <= 0 || bytes > (0x1ff * 8) / ratio)
        return -1;
    return (bytes * ratio + 7) / 8;
}

static inline int
cirrus_start_encode(int bytes, int ratio)
{
    if (bytes < 0 || ratio <= 0 || bytes > (0xfffff * 4 + 3) / ratio)
        return -1;
    return bytes * ratio / 4;
}

#endif
