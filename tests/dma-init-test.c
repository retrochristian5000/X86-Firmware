// SPDX-License-Identifier: LGPL-3.0-or-later
#include <assert.h>
#include <stdint.h>

struct dma_write {
    uint16_t port;
    uint8_t value;
};

static struct dma_write writes[16];
static unsigned count;

static void outb(uint8_t value, uint16_t port)
{
    assert(count < sizeof(writes) / sizeof(writes[0]));
    writes[count++] = (struct dma_write){ port, value };
}

#include "../src/hw/dma-init.h"

int main(void)
{
    static const struct dma_write expected[] = {
        { 0xda, 0 }, { 0x0d, 0 },
        { 0x0b, 0 }, { 0x0b, 1 }, { 0x0b, 2 }, { 0x0b, 3 },
        { 0xd6, 1 }, { 0xd6, 2 }, { 0xd6, 3 },
        { 0xd6, 0xc0 }, { 0xd4, 0 },
    };

    // The same POST sequence must be safe on repeated initialization.
    for (unsigned run = 0; run < 2; run++) {
        count = 0;
        dma_init_controllers();
        assert(count == sizeof(expected) / sizeof(expected[0]));
        for (unsigned i = 0; i < count; i++) {
            assert(writes[i].port == expected[i].port);
            assert(writes[i].value == expected[i].value);
        }
    }
    return 0;
}
