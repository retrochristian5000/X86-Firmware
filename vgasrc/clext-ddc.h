// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef CLEXT_DDC_H
#define CLEXT_DDC_H

// GD5446 TRM 8.3 and Appendix B8: SR8 drives open-collector DDC pins.
#define CLEXT_DDC_ENABLE 0x40
#define CLEXT_DDC_SCL 0x01
#define CLEXT_DDC_SDA 0x02
#define CLEXT_DDC_SCL_IN 0x04
#define CLEXT_DDC_SDA_IN 0x80

static void
clext_ddc_lines(u8 lines)
{
    stdvga_sequ_write(8, CLEXT_DDC_ENABLE | lines);
}

static int
clext_ddc_clock(u8 data)
{
    clext_ddc_lines(data);
    clext_ddc_lines(data | CLEXT_DDC_SCL);
    // Bound the wait even if the monitor holds SCL low.
    int i;
    for (i = 0; i < 256; i++)
        if (stdvga_sequ_read(8) & CLEXT_DDC_SCL_IN)
            return 0;
    return -1;
}

static int
clext_ddc_start(void)
{
    if (clext_ddc_clock(CLEXT_DDC_SDA))
        return -1;
    if (!(stdvga_sequ_read(8) & CLEXT_DDC_SDA_IN))
        return -1;
    clext_ddc_lines(CLEXT_DDC_SCL);
    clext_ddc_lines(0);
    return 0;
}

static void
clext_ddc_stop(void)
{
    if (!clext_ddc_clock(0))
        clext_ddc_lines(CLEXT_DDC_SCL | CLEXT_DDC_SDA);
}

static int
clext_ddc_send(u8 value)
{
    int i;
    for (i = 0; i < 8; i++) {
        u8 data = value & 0x80 ? CLEXT_DDC_SDA : 0;
        if (clext_ddc_clock(data))
            return -1;
        clext_ddc_lines(data);
        value <<= 1;
    }
    if (clext_ddc_clock(CLEXT_DDC_SDA))
        return -1;
    int nack = stdvga_sequ_read(8) & CLEXT_DDC_SDA_IN;
    clext_ddc_lines(CLEXT_DDC_SDA);
    return nack ? -1 : 0;
}

static int
clext_ddc_recv(u8 *value, int last)
{
    u8 byte = 0;
    int i;
    for (i = 0; i < 8; i++) {
        if (clext_ddc_clock(CLEXT_DDC_SDA))
            return -1;
        byte = (byte << 1) | !!(stdvga_sequ_read(8) & CLEXT_DDC_SDA_IN);
        clext_ddc_lines(CLEXT_DDC_SDA);
    }
    u8 data = last ? CLEXT_DDC_SDA : 0;
    if (clext_ddc_clock(data))
        return -1;
    clext_ddc_lines(data);
    *value = byte;
    return 0;
}

static int
clext_ddc_valid(const u8 *edid)
{
    int i;
    u8 sum = 0;
    if (edid[0] || edid[7])
        return 0;
    for (i = 1; i < 7; i++)
        if (edid[i] != 0xff)
            return 0;
    for (i = 0; i < 128; i++)
        sum += edid[i];
    return !sum && edid[18] == 1;
}

// Read the base EDID block from the standard DDC2B monitor at 7-bit 0x50.
static int
clext_ddc_read(u8 *edid)
{
    u8 saved = stdvga_sequ_read(8);
    int ret = -1;
    int i;

    if (clext_ddc_start() || clext_ddc_send(0xa0) || clext_ddc_send(0)
        || clext_ddc_start() || clext_ddc_send(0xa1))
        goto done;
    for (i = 0; i < 128; i++)
        if (clext_ddc_recv(&edid[i], i == 127))
            goto done;
    if (clext_ddc_valid(edid))
        ret = 0;
done:
    clext_ddc_stop();
    // Restore the board configuration, leaving a DDC bus released.
    stdvga_sequ_write(8, saved & CLEXT_DDC_ENABLE ? 0x43 : saved & 0x3f);
    return ret;
}

#endif
