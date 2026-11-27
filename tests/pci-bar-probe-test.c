// SPDX-License-Identifier: LGPL-3.0-or-later
// Host regression for the production PCI BAR transaction helpers.
#include "../src/fw/pci-bar-probe.h"

#define CHECK(x) do { if (!(x)) __builtin_trap(); } while (0)
#define DECODE (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)

static u16 command;
static u32 low, high, low_mask, high_mask, offset;
static unsigned writes;
static int wide, probing;

u16 pci_config_readw(u16 bdf, u32 addr)
{
    CHECK(bdf == 0x20 && addr == PCI_COMMAND);
    return command;
}

void pci_config_writew(u16 bdf, u32 addr, u16 value)
{
    CHECK(bdf == 0x20 && addr == PCI_COMMAND);
    command = value;
    writes++;
}

u32 pci_config_readl(u16 bdf, u32 addr)
{
    CHECK(bdf == 0x20);
    if (addr == offset) {
        if (probing && low == ~0U) {
            CHECK(!(command & DECODE));
            CHECK(!wide || high == ~0U);
            return low_mask;
        }
        if (probing && offset == PCI_ROM_ADDRESS
            && low == PCI_ROM_ADDRESS_MASK) {
            CHECK(!(command & DECODE));
            return low_mask;
        }
        return low;
    }
    CHECK(wide && addr == offset + 4);
    if (probing && high == ~0U) {
        CHECK(!(command & DECODE) && low == ~0U);
        return high_mask;
    }
    return high;
}

void pci_config_writel(u16 bdf, u32 addr, u32 value)
{
    CHECK(bdf == 0x20 && !(command & DECODE));
    if (addr == offset)
        low = value;
    else {
        CHECK(wide && addr == offset + 4);
        high = value;
    }
    writes++;
}

int main(void)
{
    const u16 flags = PCI_COMMAND_MASTER | PCI_COMMAND_VGA_PALETTE;
    command = flags | DECODE;
    offset = PCI_BASE_ADDRESS_0;
    low = 0xe0000000;
    low_mask = 0xfe000000;
    probing = 1;
    CHECK(pci_bar_probe_mask(0x20, offset, ~0U, 0) == low_mask);
    CHECK(low == 0xe0000000 && command == (flags | DECODE));
    CHECK(writes == 4);

    // Relocate a 64-bit BAR without transiently enabling either half.
    wide = 1;
    low = 0x80000004;
    high = 2;
    low_mask = 0xffe00004;
    high_mask = ~0U;
    writes = 0;
    CHECK(pci_bar_probe_mask(0x20, offset, ~0U, 1)
          == 0xffffffffffe00004ULL);
    CHECK(low == 0x80000004 && high == 2 && command == (flags | DECODE));
    CHECK(writes == 6);

    probing = 0;
    writes = 0;
    pci_bar_assign(0x20, offset, 0x340000000ULL, 1);
    CHECK(low == 0x40000000 && high == 3 && command == (flags | DECODE));
    CHECK(writes == 4);

    // A disabled device stays disabled; ROM enable is restored exactly.
    command = flags;
    wide = 0;
    offset = PCI_ROM_ADDRESS;
    low = 0xc0000001;
    low_mask = 0xffff0000;
    probing = 1;
    writes = 0;
    CHECK(pci_bar_probe_mask(0x20, offset, PCI_ROM_ADDRESS_MASK, 0)
          == low_mask);
    CHECK(low == 0xc0000001 && command == flags && writes == 2);
    return 0;
}
