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

    // Exercise conversion rounding and limits without relying on MIN.
    CHECK(legacy_88_kib(0) == 0);
    CHECK(legacy_88_kib(1023) == 0);
    CHECK(legacy_88_kib(1024) == 1);
    CHECK(legacy_88_kib(63u * 1024 * 1024 - 1) == 63 * 1024 - 1);
    CHECK(legacy_88_kib(0xffffffffu) == 63 * 1024);
    CHECK(legacy_e801_low_kib(0) == 0);
    CHECK(legacy_e801_low_kib(15u * 1024 * 1024 - 1) == 15 * 1024 - 1);
    CHECK(legacy_e801_low_kib(0xffffffffu) == 15 * 1024);
    CHECK(legacy_e801_high_blocks(0) == 0);
    CHECK(legacy_e801_high_blocks(65535) == 0);
    CHECK(legacy_e801_high_blocks(65536) == 1);
    CHECK(legacy_e801_high_blocks(0xffffffffu) == 0xffff);

    // Initial and mixed shadow states, including a partial first block.
    u8 pam[7] = {0};
    CHECK(shadow_copy_mask(pam, 0xf0000, 0x100000) == 0xf000);
    CHECK(shadow_copy_mask(pam, 0xe0000, 0x100000) == 0xff00);
    CHECK(shadow_copy_mask(pam, 0xc0000, 0x100000) == 0xffff);
    pam[0] = 0x10;
    CHECK(shadow_copy_mask(pam, 0xe0100, 0x100000) == 0x0f00);
    pam[5] = 0x13;
    CHECK(shadow_copy_mask(pam, 0xe0100, 0x100000) == 0x0c00);
    pam[6] = 0x21;
    CHECK(shadow_copy_mask(pam, 0xe0100, 0x100000) == 0x0800);
    pam[6] = 0x31;
    CHECK(shadow_copy_mask(pam, 0xe0100, 0x100000) == 0);
    pam[0] = 0x20;
    CHECK(shadow_copy_mask(pam, 0xf0001, 0x100000) == 0xf000);
    CHECK(shadow_copy_mask(pam, 0xc1000, 0xc4000) == 1);
    CHECK(shadow_copy_mask(pam, 0xc4000, 0xc8000) == 2);

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
