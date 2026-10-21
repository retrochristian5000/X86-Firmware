// Support for MC146818 Real Time Clock chip.
//
// Copyright (C) 2008-2013  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2002  MandrakeSoft S.A.
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "biosvar.h" // GET_LOW
#if !MODESEGMENT
#include "romfile.h" // romfile_loadbool
#endif
#include "rtc.h" // rtc_read
#include "stacks.h" // yield
#include "util.h" // timer_calc
#include "x86.h" // inb

u8
rtc_read(u8 index)
{
    index |= NMI_DISABLE_BIT;
    outb(index, PORT_CMOS_INDEX);
    return inb(PORT_CMOS_DATA);
}

void
rtc_write(u8 index, u8 val)
{
    index |= NMI_DISABLE_BIT;
    outb(index, PORT_CMOS_INDEX);
    outb(val, PORT_CMOS_DATA);
}

void
rtc_mask(u8 index, u8 off, u8 on)
{
    index |= NMI_DISABLE_BIT;
    outb(index, PORT_CMOS_INDEX);
    u8 val = inb(PORT_CMOS_DATA);
    outb((val & ~off) | on, PORT_CMOS_DATA);
}

#if !MODESEGMENT
/*
 * AMI Hi-Flex CMOS compatibility profile (1993-era layout).
 *
 * This experimental, opt-in overlay cannot provide a complete AMI CMOS
 * layout: QEMU owns 34h-35h (extended memory size), while AMI Hi-Flex
 * assigns shadowing controls there.  Never overwrite QEMU's RAM metadata.
 * QEMU also uses 38h-3Dh for boot order and disk translation.  Preserve
 * those bytes, too.  No AMI password or shadowing behavior is emulated.
 */
#define CMOS_AMI_STD_CHECKSUM_HI  0x2e
#define CMOS_AMI_STD_CHECKSUM_LO  0x2f
/* QEMU owns 34h/35h as extended-memory size; do not write either byte. */
#define CMOS_AMI_EXT_CHECKSUM_FIRST 0x34
#define CMOS_AMI_CHIPSET          0x36
#define CMOS_AMI_PASSWORD_COLOR   0x37
#define CMOS_AMI_PASSWORD_END     0x3d
#define CMOS_AMI_EXT_CHECKSUM_HI  0x3e
#define CMOS_AMI_EXT_CHECKSUM_LO  0x3f

static u16
rtc_checksum_range(u8 first, u8 last)
{
    u16 sum = 0;
    u8 i;

    for (i = first; i <= last; i++)
        sum += rtc_read(i);
    return sum;
}

static void
ami_hiflex_cmos_setup(void)
{
    if (!romfile_loadbool("opt/org.seabios/ami-hiflex-cmos", 0))
        return;

    /* IBM/AT-compatible checksum used by AMI: additive sum of 10h-2Dh. */
    u16 sum = rtc_checksum_range(0x10, 0x2d);
    rtc_write(CMOS_AMI_STD_CHECKSUM_HI, sum >> 8);
    rtc_write(CMOS_AMI_STD_CHECKSUM_LO, sum);

    /*
     * Do not program the AMI shadowing/password bits at 34h and 35h.
     * Those offsets are QEMU's extended-memory-size bytes, and SeaBIOS
     * uses them for RAM detection when early E820 probing fails.
     * Shadowing and numeric-processor-test settings are intentionally
     * unavailable in this partial Hi-Flex overlay.
     */

    /* Chipset-specific byte is unknown for the generic SeaBIOS profile. */
    rtc_write(CMOS_AMI_CHIPSET, 0x00);

    /* Documented Hi-Flex color value: white/light-gray text on black. */
    rtc_write(CMOS_AMI_PASSWORD_COLOR, 0x07);

    /*
     * Do not clear 38h-3Dh.  SeaBIOS/QEMU still owns selected bytes there;
     * AMI treats the range as encrypted password storage, and password
     * checking above is disabled.  Include the preserved bytes in the AMI
     * extended additive checksum as the real firmware would.
     */
    sum = rtc_checksum_range(CMOS_AMI_EXT_CHECKSUM_FIRST, CMOS_AMI_PASSWORD_END);
    rtc_write(CMOS_AMI_EXT_CHECKSUM_HI, sum >> 8);
    rtc_write(CMOS_AMI_EXT_CHECKSUM_LO, sum);
}
#endif

int
rtc_updating(void)
{
    // This function checks to see if the update-in-progress bit
    // is set in CMOS Status Register A.  If not, it returns 0.
    // If it is set, it tries to wait until there is a transition
    // to 0, and will return 0 if such a transition occurs.  A -1
    // is returned only after timing out.  The maximum period
    // that this bit should be set is constrained to (1984+244)
    // useconds, but we wait for longer just to be sure.

    if ((rtc_read(CMOS_STATUS_A) & RTC_A_UIP) == 0)
        return 0;
    u32 end = timer_calc(15);
    for (;;) {
        if ((rtc_read(CMOS_STATUS_A) & RTC_A_UIP) == 0)
            return 0;
        if (timer_check(end))
            // update-in-progress never transitioned to 0
            return -1;
        yield();
    }
}

void
rtc_setup(void)
{
    if (CONFIG_RTC_TIMER) {
        rtc_write(CMOS_STATUS_A, 0x26); // 32,768Khz src, 976.5625us updates
        rtc_mask(CMOS_STATUS_B, ~RTC_B_DSE, RTC_B_24HR);
        rtc_read(CMOS_STATUS_C);
        rtc_read(CMOS_STATUS_D);
    }
#if !MODESEGMENT
    ami_hiflex_cmos_setup();
#endif
}

int RTCusers VARLOW;

void
rtc_use(void)
{
    if (!CONFIG_RTC_TIMER)
        return;
    int count = GET_LOW(RTCusers);
    SET_LOW(RTCusers, count+1);
    if (count)
        return;
    // Turn on the Periodic Interrupt timer
    rtc_mask(CMOS_STATUS_B, 0, RTC_B_PIE);
}

void
rtc_release(void)
{
    if (!CONFIG_RTC_TIMER)
        return;
    int count = GET_LOW(RTCusers);
    SET_LOW(RTCusers, count-1);
    if (count != 1)
        return;
    // Clear the Periodic Interrupt.
    rtc_mask(CMOS_STATUS_B, RTC_B_PIE, 0);
}
