// SPDX-License-Identifier: LGPL-3.0-or-later
// Host-executable regression checks for the same pure helpers as POST.
#include "post-selftest.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

int
main(void)
{
    volatile u32 scratch[1024];
    struct e820entry entries[] = {
        { 0, 0x9fc00, E820_RAM },
        { 0x9fc00, 0x400, E820_RESERVED },
        { 0x100000, 0x3f00000, E820_RAM },
    };

    whp_post_ram_fill(scratch, 1024, 0);
    CHECK(whp_post_ram_verify(scratch, 1024, 0) == 0);
    scratch[513] ^= 1U;
    CHECK(whp_post_ram_verify(scratch, 1024, 0) != 0);
    whp_post_ram_fill(scratch, 1024, 1);
    CHECK(whp_post_ram_verify(scratch, 1024, 1) == 0);
    CHECK(whp_post_ram_verify(scratch, 1024, 0) != 0);
    CHECK(whp_post_ram_walk(&scratch[0]) == 0);

    CHECK(whp_post_e820_valid(entries, 3) == 0);
    CHECK(whp_post_e820_reserves(entries, 3, 0x9fc00, 1024));
    CHECK(!whp_post_e820_reserves(entries, 3, 0x90000, 1024));
    entries[1].start = 0x9fb00;  // Overlap
    CHECK(whp_post_e820_valid(entries, 3) != 0);
    entries[1].start = 0x9fc00;
    entries[1].size = 0;  // Zero size
    CHECK(whp_post_e820_valid(entries, 3) != 0);
    entries[1].size = 0x400;
    entries[2].start = ~0ULL - 10ULL;
    entries[2].size = 0x100;  // Overflow
    CHECK(whp_post_e820_valid(entries, 3) != 0);
    entries[2].start = 0x100000;
    entries[2].size = 0x3f00000;
    entries[0].type = E820_RESERVED;  // No RAM reported
    entries[2].type = E820_RESERVED;
    CHECK(whp_post_e820_valid(entries, 3) != 0);
    CHECK(whp_post_e820_valid(entries, 0) != 0);
    return 0;
}
