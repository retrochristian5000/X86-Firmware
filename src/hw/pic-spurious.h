// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __PIC_SPURIOUS_H
#define __PIC_SPURIOUS_H

// The 8259A reports a default IR7 vector if the request disappears
// before acknowledgment. An ISR bit, not the vector alone, identifies
// a real interrupt. Use specific EOIs to avoid clearing another ISR bit.
static inline void
pic_finish_irq7(unsigned int isr)
{
    if (isr & 0x80)
        outb(0x67, PORT_PIC1_CMD);
}

static inline void
pic_finish_irq15(unsigned int slave_isr)
{
    if (slave_isr & 0x80)
        outb(0x67, PORT_PIC2_CMD);
    // Even a spurious slave vector was acknowledged through master IR2.
    outb(0x62, PORT_PIC1_CMD);
}

#endif
