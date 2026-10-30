// Validated APM 1.2 POST snapshot for future laptop emulation.
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __APM_POWER_H
#define __APM_POWER_H
#include "types.h" // u8

/* Exactly eight fw_cfg bytes; the remaining-time word is little-endian. */
#define WHP_APM_POWER_VERSION 1
#define WHP_APM_POWER_FILE "opt/org.seabios/whp-laptop-apm"
struct apm_power_snapshot {
    u8 version, ac_line, battery_status, battery_flags;
    u8 percentage, units, time_lo, time_hi;
} PACKED;

static inline int
apm_power_valid(const struct apm_power_snapshot *s)
{
    if (s->version != WHP_APM_POWER_VERSION
        || (s->ac_line > 2 && s->ac_line != 0xff)
        || (s->battery_status > 3 && s->battery_status != 0xff)
        || (s->battery_flags != 0xff && (s->battery_flags & 0x60))
        || (s->percentage > 100 && s->percentage != 0xff)
        || s->units > 1)
        return 0;
    if (s->units == 0)
        return s->battery_status == 0xff && s->battery_flags == 0x80
               && s->percentage == 0xff && s->time_lo == 0xff
               && s->time_hi == 0xff;
    if (s->battery_flags != 0xff && (s->battery_flags & 0x90))
        return 0;
    if (s->battery_status != 0xff && s->battery_flags != 0xff
        && !(s->battery_flags & (1 << s->battery_status)))
        return 0;
    return 1;
}
#endif
