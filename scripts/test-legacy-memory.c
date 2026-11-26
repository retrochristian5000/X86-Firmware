// SPDX-License-Identifier: LGPL-3.0-or-later
#include "legacy-memory.h"
#include "fw/shadow-umb.h"

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

int main(void)
{
    const struct e820entry ordinary[] = {
        {0, 0xa0000, E820_RAM},
        {0x100000, 0x3f00000, E820_RAM},
    };
    const struct e820entry isa_hole[] = {
        {0x100000, 0xe00000, E820_RAM},
        {0xf00000, 0x100000, E820_RESERVED},
        {0x1000000, 0x3000000, E820_RAM},
    };
    const struct e820entry acpi_hole[] = {
        {0x100000, 0x700000, E820_RAM},
        {0x800000, 0x1000, E820_ACPI},
        {0x801000, 0x7ff000, E820_RAM},
        {0x1000000, 0x1000000, E820_RAM},
        {0x2000000, 0x1000, E820_NVS},
    };
    const struct e820entry at_4g[] = {
        {0x100000, 0x100000000ull, E820_RAM},
    };
    u32 low = legacy_contiguous_ram(ordinary, ARRAY_SIZE(ordinary),
                                    LEGACY_ONE_MIB);
    CHECK(legacy_88_kib(low) == 63 * 1024);
    CHECK(legacy_e801_low_kib(low) == 15 * 1024);
    CHECK(legacy_e801_high_blocks(legacy_contiguous_ram(
        ordinary, ARRAY_SIZE(ordinary), LEGACY_16_MIB)) == 768);
    low = legacy_contiguous_ram(isa_hole, ARRAY_SIZE(isa_hole),
                                LEGACY_ONE_MIB);
    CHECK(legacy_88_kib(low) == 14 * 1024);
    CHECK(legacy_e801_low_kib(low) == 14 * 1024);
    CHECK(legacy_e801_high_blocks(legacy_contiguous_ram(
        isa_hole, ARRAY_SIZE(isa_hole), LEGACY_16_MIB)) == 768);
    CHECK(legacy_contiguous_ram(acpi_hole, ARRAY_SIZE(acpi_hole),
                               LEGACY_ONE_MIB) == 0x700000);
    CHECK(legacy_contiguous_ram(acpi_hole, ARRAY_SIZE(acpi_hole),
                               LEGACY_16_MIB) == 0x1000000);
    CHECK(legacy_contiguous_ram(ordinary, 0, LEGACY_ONE_MIB) == 0);
    CHECK(legacy_contiguous_ram(at_4g, ARRAY_SIZE(at_4g),
                               LEGACY_16_MIB) == 0xff000000u);
    CHECK(legacy_e801_high_blocks(0xff000000u) == 65280);

    // Default: unused and ROM blocks read-only; live data writable.
    CHECK(shadow_umb_pam(0xc0000, 0xc6000, 0xe4000, 0xec000,
                         1, 0) == 0x11);
    CHECK(shadow_umb_pam(0xe0000, 0xc6000, 0xe4000, 0xec000,
                         1, 0) == 0x31);
    CHECK(shadow_umb_pam(0xe8000, 0xc6000, 0xe4000, 0xec000,
                         1, 0) == 0x13);
    // Explicit writable UMB policy must not expose ROM or BIOS code.
    CHECK(shadow_umb_pam(0xc0000, 0xc6000, 0xe4000, 0xec000,
                         1, 1) == 0x11);
    CHECK(shadow_umb_pam(0xc8000, 0xc6000, 0xe4000, 0xec000,
                         1, 1) == 0x33);
    CHECK(shadow_umb_pam(0xe8000, 0xc6000, 0xec000, 0xec000,
                         0, 1) == 0x13);
    CHECK(shadow_umb_pam(0xe0000, 0xc6000, 0xe6000, 0xe9000,
                         1, 0) == 0x31);
    CHECK(shadow_umb_pam(0xe8000, 0xc6000, 0xe6000, 0xe9000,
                         1, 0) == 0x13);
    return 0;
}
