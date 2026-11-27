/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* CT1741 command and playback firmware for the MCS-51 core. */
#include "command.h"

/*
 * Provisional internal interface, corroborated by MAME's CT1741 model.
 * P1 is the MCU special-function register, not an ISA port.
 * The XDATA latch is read for host input and written for host output.
 */
__sfr __at (0x90) ct1741_p1;
__xdata __at (0x0000) volatile uint8_t ct1741_data;

/* Private v1 hardware-control mailbox, not a physical CT1741 pinout. */
__xdata __at (0x0100) volatile uint8_t ct1741_action;
__xdata __at (0x0101) volatile uint8_t ct1741_arg0;
__xdata __at (0x0102) volatile uint8_t ct1741_arg1;
__xdata __at (0x0103) volatile uint8_t ct1741_arg2;
__xdata __at (0x0104) volatile uint8_t ct1741_arg3;
/* Optional completion handshake: A5h enables it, 00h completes a byte. */
__xdata __at (0x0105) volatile uint8_t ct1741_completion;

#define HOST_OUTPUT_FULL 0x01
#define HOST_INPUT_READY 0x02

static CT1741Command command;

void main(void)
{
    uint8_t response;

    ct1741_p1 = 0xff; /* Release quasi-bidirectional input pins. */
    ct1741_reset(&command);
    ct1741_completion = 0xa5;

    for (;;) {
        uint8_t status = ct1741_p1;

        if (status & HOST_OUTPUT_FULL)
            continue;

        if (ct1741_take(&command, &response)) {
            ct1741_data = response;
            continue;
        }

        if (status & HOST_INPUT_READY) {
            uint8_t action;
            ct1741_accept(&command, ct1741_data);
            action = ct1741_take_action(&command);
            if (action) {
                ct1741_arg0 = command.args[0];
                ct1741_arg1 = command.args[1];
                ct1741_arg2 = command.args[2];
                ct1741_arg3 = command.args[3];
                ct1741_action = action;
            }
            ct1741_completion = 0;
        }
    }
}
