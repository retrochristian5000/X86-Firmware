#include <stdint.h>
#include <string.h>

static unsigned char TestHeap[256];

void *malloc_tmphigh(u32 size)
{
    return size <= sizeof(TestHeap) ? TestHeap : 0;
}

static int check(int condition, int code)
{
    return condition ? 0 : code;
}

static void setle16(u8 *p, u16 value)
{
    p[0] = value;
    p[1] = value >> 8;
}

static int build_valid_pcx(u8 *data, int size)
{
    if (size < 901)
        return -1;
    memset(data, 0, size);
    data[0] = 0x0a;
    data[1] = 5;
    data[2] = 1;
    data[3] = 8;
    setle16(data + 8, 1);
    setle16(data + 10, 1);
    data[65] = 1;
    setle16(data + 66, 2);
    setle16(data + 68, 1);

    data[128] = 0xc2;
    data[129] = 1;
    data[130] = 2;
    data[131] = 3;

    data[132] = 0x0c;
    data[133 + 3] = 255;
    data[133 + 6 + 1] = 255;
    data[133 + 9 + 2] = 255;
    return 0;
}

int main(void)
{
    u8 data[901];
    if (build_valid_pcx(data, sizeof(data)))
        return 1;

    struct pcx_decdata *pcx = pcx_alloc();
    if (check(pcx != 0, 2))
        return 2;
    if (check(pcx_decode(pcx, data, sizeof(data)) == 0, 3))
        return 3;

    int width = 0, height = 0;
    pcx_get_info(pcx, &width, &height);
    if (check(width == 2 && height == 2, 4))
        return 4;

    u8 out[12];
    memset(out, 0xaa, sizeof(out));
    if (check(pcx_show(pcx, out, 2, 2, 24, 6) == 0, 5))
        return 5;
    const u8 expected[12] = {
        0, 0, 255, 0, 0, 255,
        0, 255, 0, 255, 0, 0,
    };
    if (check(memcmp(out, expected, sizeof(out)) == 0, 6))
        return 6;

    u8 out16[8];
    memset(out16, 0xaa, sizeof(out16));
    if (check(pcx_show(pcx, out16, 2, 2, 16, 4) == 0, 10))
        return 10;
    const u8 expected16[8] = {
        0x00, 0xf8, 0x00, 0xf8,
        0xe0, 0x07, 0x1f, 0x00,
    };
    if (check(memcmp(out16, expected16, sizeof(out16)) == 0, 11))
        return 11;

    u8 out32[16];
    memset(out32, 0xaa, sizeof(out32));
    if (check(pcx_show(pcx, out32, 2, 2, 32, 8) == 0, 12))
        return 12;
    const u8 expected32[16] = {
        255, 0, 0, 0, 255, 0, 0, 0,
        0, 255, 0, 0, 0, 0, 255, 0,
    };
    if (check(memcmp(out32, expected32, sizeof(out32)) == 0, 13))
        return 13;

    data[0] = 0;
    if (check(pcx_decode(pcx, data, sizeof(data)) != 0, 7))
        return 7;
    build_valid_pcx(data, sizeof(data));
    data[132] = 0;
    if (check(pcx_decode(pcx, data, sizeof(data)) != 0, 8))
        return 8;
    build_valid_pcx(data, sizeof(data));
    data[128] = 0xff;
    if (check(pcx_decode(pcx, data, sizeof(data)) != 0, 9))
        return 9;

    return 0;
}
