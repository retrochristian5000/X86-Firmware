// Optional Sound Blaster 16 DSP diagnostic for QEMU development.
// This file may be distributed under the terms of the GNU LGPLv3 license.
//
// Creative Sound Blaster Series Hardware Programming Guide, chapters 2, 6.
#include "config.h"
#include "hw/sb16-post.h"
#include "output.h"
#include "romfile.h"
#include "util.h"
#include "x86.h"

#if CONFIG_SB16_POST_DIAGNOSTIC
#define SB16_BASE 0x220
#define SB16_RESET (SB16_BASE + 6)
#define SB16_READ (SB16_BASE + 0xa)
#define SB16_WRITE (SB16_BASE + 0xc)
#define SB16_READ_STATUS (SB16_BASE + 0xe)
#define SB16_TIMEOUT_MS 10

static int
sb16_wait_status(u16 port, int ready)
{
    u32 end = timer_calc(SB16_TIMEOUT_MS);
    do {
        if (!!(inb(port) & 0x80) == ready)
            return 0;
        cpu_relax();
    } while (!timer_check(end));
    return -1;
}

static int
sb16_dsp_write(u8 value)
{
    if (sb16_wait_status(SB16_WRITE, 0))
        return -1;
    outb(value, SB16_WRITE);
    return 0;
}

static int
sb16_dsp_read(u8 *value)
{
    if (sb16_wait_status(SB16_READ_STATUS, 1))
        return -1;
    *value = inb(SB16_READ);
    return 0;
}

void
sb16_post_diagnostic(void)
{
    u8 response, major, minor;
    const char *phase = "reset acknowledgement";

    // A DSP reset changes device state. Never scan ISA ports or reset a
    // sound card unless the operator explicitly requests this test.
    if (!romfile_loadbool("opt/org.seabios/whp-sb16-post", 0))
        return;

    outb(1, SB16_RESET);
    udelay(3);
    outb(0, SB16_RESET);
    if (sb16_dsp_read(&response) || response != 0xaa)
        goto fail;

    phase = "DSP version";
    if (sb16_dsp_write(0xe1) || sb16_dsp_read(&major) ||
        sb16_dsp_read(&minor))
        goto fail;

    // This test targets the SB16 DSP family, not earlier Sound Blasters.
    if (major != 4)
        goto fail;

    phase = "DSP identification";
    if (sb16_dsp_write(0xe0) || sb16_dsp_write(0x55) ||
        sb16_dsp_read(&response) || response != 0xaa)
        goto fail;

    dprintf(1, "SB16 POST: DSP %u.%02u at 220h, reset and identification PASS\n",
            major, minor);
    return;

fail:
    dprintf(1, "SB16 POST: %s failed at 220h; continuing boot\n", phase);
}
#endif
