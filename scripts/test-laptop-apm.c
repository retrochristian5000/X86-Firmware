// Exercise the actual APM record validator used in the firmware.
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "apm-power.h"
int main(void)
{
    struct apm_power_snapshot s = {1, 1, 3, 8, 67, 1, 0x2c, 0x81};
    if (sizeof(s) != 8 || !apm_power_valid(&s)) return 1;
    s.version = 2;
    if (apm_power_valid(&s)) return 2;
    s.version = 1;
    s.ac_line = 3;
    if (apm_power_valid(&s)) return 3;
    s.ac_line = 0;
    s.percentage = 101;
    if (apm_power_valid(&s)) return 4;
    s.percentage = 40;
    s.battery_flags = 0x40;
    if (apm_power_valid(&s)) return 5;
    s.battery_flags = 8;
    s.battery_status = 0;
    if (apm_power_valid(&s)) return 6;
    s.battery_status = 3;
    s.units = 2;
    if (apm_power_valid(&s)) return 7;
    s.units = 1;
    s.battery_flags = 0x80;
    if (apm_power_valid(&s)) return 8;
    s.battery_flags = 8;
    s.units = 0;
    if (apm_power_valid(&s)) return 9;
    s.battery_status = 0xff;
    s.battery_flags = 0x80;
    s.percentage = s.time_lo = s.time_hi = 0xff;
    if (!apm_power_valid(&s)) return 10;
    s.battery_flags = 0xff;
    if (apm_power_valid(&s)) return 11;
    s.units = 1;
    if (!apm_power_valid(&s)) return 12;
    return 0;
}
