/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* CT1741 identification firmware; compiled only for the MCS-51 core. */
#include "command.h"

/*
 * Provisional internal interface, corroborated by MAME's CT1741 model.
 * P1 is the MCU special-function register, not an ISA port.
 * The XDATA latch is read for host input and written for host output.
 */
__sfr __at (0x90) ct1741_p1;
__xdata __at (0x0000) volatile uint8_t ct1741_data;

#define HOST_OUTPUT_FULL 0x01
#define HOST_INPUT_READY 0x02

static CT1741Command command;

void main(void)
{
    uint8_t response;

    ct1741_p1 = 0xff; /* Release quasi-bidirectional input pins. */
    ct1741_reset(&command);

    for (;;) {
        uint8_t status = ct1741_p1;

        if (status & HOST_OUTPUT_FULL)
            continue;

        if (ct1741_take(&command, &response)) {
            ct1741_data = response;
            continue;
        }

        if (status & HOST_INPUT_READY)
            ct1741_accept(&command, ct1741_data);
    }
}
