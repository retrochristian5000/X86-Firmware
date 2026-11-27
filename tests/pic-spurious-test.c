// SPDX-License-Identifier: LGPL-3.0-or-later
#include <assert.h>
#include <stdint.h>

#define PORT_PIC1_CMD 0x20
#define PORT_PIC2_CMD 0xa0

static struct {
    uint16_t port;
    uint8_t value;
} writes[2];
static unsigned int count;

static void outb(uint8_t value, uint16_t port)
{
    assert(count < 2);
    writes[count].port = port;
    writes[count++].value = value;
}

#include "../src/hw/pic-spurious.h"

int main(void)
{
    // Exercise every ISR bitmap, including unrelated in-service IRQs.
    for (unsigned int isr = 0; isr < 256; isr++) {
        count = 0;
        pic_finish_irq7(isr);
        assert(count == !!(isr & 0x80));
        if (count) {
            assert(writes[0].port == PORT_PIC1_CMD);
            assert(writes[0].value == 0x67);
        }

        count = 0;
        pic_finish_irq15(isr);
        assert(count == 1U + !!(isr & 0x80));
        if (count == 2) {
            assert(writes[0].port == PORT_PIC2_CMD);
            assert(writes[0].value == 0x67);
        }
        assert(writes[count - 1].port == PORT_PIC1_CMD);
        assert(writes[count - 1].value == 0x62);
    }
    return 0;
}
