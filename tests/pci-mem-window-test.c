// SPDX-License-Identifier: LGPL-3.0-or-later
#include "../src/fw/pci-mem-window.h"

#define CHECK(x) do { if (!(x)) __builtin_trap(); } while (0)
#define M (0x100000ULL)

int main(void)
{
    struct pci_mem_window w;
    const struct e820entry map[] = {
        {0, 640 * 1024, E820_RAM},
        {0xa0000, 0x60000, E820_RESERVED},
        {M, 14 * M, E820_RAM},
        {15 * M, M, E820_RESERVED},
        {0x80000000ULL, 0x20000000ULL, E820_RESERVED},
        {0xe0000000ULL, 32 * M, E820_RESERVED},
    };
    CHECK(!pci_mem32_window(map, 6, 0, 0x100000000ULL,
                            16 * M, 16 * M, 32 * M, 32 * M, &w));
    CHECK(w.start == 16 * M && w.end == 0x80000000ULL);
    CHECK(w.upper_base == 0x7e000000ULL && w.lower_base == 0x7d000000ULL);

    // A reservation splits the normal PCI hole; both classes fit below it.
    CHECK(!pci_mem32_window(map, 6, 0xc0000000ULL, 0xfec00000ULL,
                            16 * M, 16 * M, 32 * M, 32 * M, &w));
    CHECK(w.start == 0xc0000000ULL && w.end == 0xe0000000ULL);
    CHECK(w.upper_base == 0xde000000ULL && w.lower_base == 0xdd000000ULL);

    // Preserve the original aperture on a standard unoccupied machine.
    CHECK(!pci_mem32_window(map, 4, 0xe0000000ULL, 0xfec00000ULL,
                            16 * M, 16 * M, 32 * M, 32 * M, &w));
    CHECK(w.start == 0xe0000000ULL && w.end == 0xfec00000ULL);

    // No room, subtraction wrap, alignment padding, and invalid alignment.
    CHECK(pci_mem_pack(16 * M, 32 * M, 16 * M, M, M, M, &w));
    CHECK(pci_mem_pack(16 * M, 32 * M, ~0ULL, M, 0, M, &w));
    CHECK(pci_mem_pack(17 * M, 32 * M, 0, M, 15 * M, 16 * M, &w));
    CHECK(pci_mem_pack(16 * M, 32 * M, M, 3, M, M, &w));
    CHECK(!pci_mem_pack(16 * M, 32 * M, 8 * M, M, 8 * M, M, &w));
    CHECK(w.lower_base == 16 * M && w.upper_base == 24 * M);
    CHECK(pci_mem32_window(map, 6, 0x80000000ULL, 0xa0000000ULL,
                           M, M, M, M, &w));
    CHECK(pci_mem32_window(map, 6, 0, 16 * M, 0, 1, 0, 1, &w));
    const struct e820entry overlap[] = {
        {16 * M, 16 * M, E820_RAM},
        {24 * M, M, E820_RESERVED},
    };
    const struct e820entry wrap[] = {{~0ULL - M, 2 * M, E820_RESERVED}};
    CHECK(pci_mem32_window(overlap, 2, 16 * M, 64 * M, M, M, M, M, &w));
    CHECK(pci_mem32_window(wrap, 1, 16 * M, 64 * M, M, M, M, M, &w));
    // Equal-size gaps prefer the higher address. Empty entries do not
    // affect the traversal; malformed entries outside the window fail.
    const struct e820entry tie[] = {
        {32 * M, 16 * M, E820_RESERVED},
        {49 * M, 0, E820_RESERVED},
    };
    CHECK(!pci_mem32_window(tie, 2, 16 * M, 64 * M, M, M, M, M, &w));
    CHECK(w.start == 48 * M && w.end == 64 * M);
    const struct e820entry outside[] = {
        {16 * M, M, E820_RAM},
        {128 * M, 16 * M, E820_RAM},
        {129 * M, M, E820_RESERVED},
    };
    CHECK(pci_mem32_window(outside, 3, 16 * M, 64 * M, M, M, M, M, &w));
    CHECK(!pci_mem32_window(0, 0, 16 * M, 64 * M, M, M, M, M, &w));
    CHECK(w.start == 16 * M && w.end == 64 * M);
    return 0;
}
