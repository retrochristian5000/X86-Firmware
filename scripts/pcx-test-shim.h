#ifndef __PCX_TEST_SHIM_H
#define __PCX_TEST_SHIM_H
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

void *malloc_tmphigh(u32 size);

struct pcx_decdata;
struct pcx_decdata *pcx_alloc(void);
int pcx_decode(struct pcx_decdata *pcx, unsigned char *data, int data_size);
void pcx_get_info(struct pcx_decdata *pcx, int *width, int *height);
int pcx_show(struct pcx_decdata *pcx, unsigned char *pic, int width,
             int height, int depth, int bytes_per_line_dest);
#endif
