/*
 * Basic ZSoft PCX decoder for boot splash images.
 *
 * Supports 8-bit, one-plane RLE PCX files with a 256-color VGA palette.
 *
 * This work is licensed under the terms of the GNU LGPLv3.
 */
#include "malloc.h" // malloc_tmphigh
#include "util.h" // struct pcx_decdata

#define PCX_HEADER_SIZE 128
#define PCX_PALETTE_SIZE 768
#define PCX_PALETTE_MARKER 0x0c

struct pcx_decdata {
    unsigned char *image;
    unsigned char *image_end;
    unsigned char *palette;
    int width;
    int height;
    int bytes_per_line;
};

static u16
pcx_getle16(const unsigned char *p)
{
    return p[0] | (p[1] << 8);
}

static int
pcx_next_run(unsigned char **src, unsigned char *end, int *count, u8 *value)
{
    if (*src >= end)
        return 1;

    u8 token = *(*src)++;
    if ((token & 0xc0) != 0xc0) {
        *count = 1;
        *value = token;
        return 0;
    }

    *count = token & 0x3f;
    if (!*count || *src >= end)
        return 1;
    *value = *(*src)++;
    return 0;
}

struct pcx_decdata *
pcx_alloc(void)
{
    return malloc_tmphigh(sizeof(struct pcx_decdata));
}

int
pcx_decode(struct pcx_decdata *pcx, unsigned char *data, int data_size)
{
    if (!pcx || !data || data_size < PCX_HEADER_SIZE + 1 + PCX_PALETTE_SIZE)
        return 1;
    if (data[0] != 0x0a)
        return 2;
    if (data[2] != 1)
        return 3;
    if (data[3] != 8 || data[65] != 1)
        return 4;

    u16 xmin = pcx_getle16(data + 4);
    u16 ymin = pcx_getle16(data + 6);
    u16 xmax = pcx_getle16(data + 8);
    u16 ymax = pcx_getle16(data + 10);
    if (xmax < xmin || ymax < ymin)
        return 5;

    u32 width = (u32)xmax - xmin + 1;
    u32 height = (u32)ymax - ymin + 1;
    u32 bytes_per_line = pcx_getle16(data + 66);
    if (!bytes_per_line || bytes_per_line < width)
        return 6;
    if (height > 0xffffffffU / bytes_per_line)
        return 7;

    int palette_offset = data_size - (1 + PCX_PALETTE_SIZE);
    if (palette_offset < PCX_HEADER_SIZE
        || data[palette_offset] != PCX_PALETTE_MARKER)
        return 8;

    unsigned char *src = data + PCX_HEADER_SIZE;
    unsigned char *image_end = data + palette_offset;
    u32 remaining = height * bytes_per_line;
    while (remaining) {
        int count;
        u8 value;
        if (pcx_next_run(&src, image_end, &count, &value))
            return 9;
        if ((u32)count > remaining)
            return 10;
        remaining -= count;
    }
    if (src != image_end)
        return 11;

    pcx->image = data + PCX_HEADER_SIZE;
    pcx->image_end = image_end;
    pcx->palette = data + palette_offset + 1;
    pcx->width = width;
    pcx->height = height;
    pcx->bytes_per_line = bytes_per_line;
    return 0;
}

void
pcx_get_info(struct pcx_decdata *pcx, int *width, int *height)
{
    *width = pcx->width;
    *height = pcx->height;
}

static void
pcx_store_pixel(unsigned char *dest, int depth, const unsigned char *rgb)
{
    u8 red = rgb[0];
    u8 green = rgb[1];
    u8 blue = rgb[2];

    if (depth == 16) {
        u16 pixel = ((red & 0xf8) << 8) | ((green & 0xfc) << 3)
                    | (blue >> 3);
        dest[0] = pixel;
        dest[1] = pixel >> 8;
    } else if (depth == 24) {
        dest[0] = blue;
        dest[1] = green;
        dest[2] = red;
    } else {
        dest[0] = red;
        dest[1] = green;
        dest[2] = blue;
        dest[3] = 0;
    }
}

int
pcx_show(struct pcx_decdata *pcx, unsigned char *pic, int width,
         int height, int depth, int bytes_per_line_dest)
{
    if (!pcx || !pic || width != pcx->width || height != pcx->height)
        return 1;
    if (depth != 16 && depth != 24 && depth != 32)
        return 2;

    int bytes_per_pixel = depth / 8;
    if (width > bytes_per_line_dest / bytes_per_pixel)
        return 3;

    unsigned char *src = pcx->image;
    int row = 0;
    int column = 0;
    while (row < height) {
        int count;
        u8 value;
        if (pcx_next_run(&src, pcx->image_end, &count, &value))
            return 4;
        while (count--) {
            if (column < width) {
                unsigned char *dest = pic + row * bytes_per_line_dest
                                      + column * bytes_per_pixel;
                pcx_store_pixel(dest, depth, pcx->palette + value * 3);
            }
            column++;
            if (column == pcx->bytes_per_line) {
                column = 0;
                row++;
                if (row == height && count)
                    return 5;
            }
        }
    }

    return src == pcx->image_end ? 0 : 6;
}
