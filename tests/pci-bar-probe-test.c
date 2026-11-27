// SPDX-License-Identifier: LGPL-3.0-or-later
// Host regression for the production PCI BAR transaction helpers.
#include "../src/fw/pci-bar-probe.h"
#include "../vgasrc/clext-pci.h"

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
            && low == (u32)PCI_ROM_ADDRESS_MASK) {
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
    CHECK(pci_bar_probe_mask(0x20, offset, (u32)PCI_ROM_ADDRESS_MASK, 0)
          == low_mask);
    CHECK(low == 0xc0000001 && command == flags && writes == 2);
    // Mask widths, including both Cirrus aperture sizes.
    CHECK(pci_bar_size(0xff000008U, PCI_BASE_ADDRESS_MEM_MASK, 0)
          == 0x01000000);
    CHECK(pci_bar_size(0xfe000000U, PCI_BASE_ADDRESS_MEM_MASK, 0)
          == 0x02000000);
    CHECK(pci_bar_size(0xfff00000U, PCI_BASE_ADDRESS_MEM_MASK, 0)
          == 0x00100000);
    CHECK(pci_bar_size(0xfff10000U, PCI_BASE_ADDRESS_MEM_MASK, 0) == 0);
    CHECK(pci_bar_size(0, PCI_BASE_ADDRESS_MEM_MASK, 0) == 0);
    CHECK(pci_bar_size(~0U, PCI_BASE_ADDRESS_MEM_MASK, 0) == 0);
    CHECK(pci_bar_size(0x0000ff01U, PCI_BASE_ADDRESS_IO_MASK, 0)
          == 0x100);
    CHECK(pci_bar_size(0x0000fffdU, PCI_BASE_ADDRESS_IO_MASK, 0)
          == 4);
    CHECK(pci_bar_size(0xffffff01U, PCI_BASE_ADDRESS_IO_MASK, 0)
          == 0x100);
    CHECK(pci_bar_size(1, PCI_BASE_ADDRESS_IO_MASK, 0) == 0);
    CHECK(pci_bar_size(0x0000f101U, PCI_BASE_ADDRESS_IO_MASK, 0) == 0);
    CHECK(pci_bar_size(0, ~0x0fULL, 1) == 0);
    CHECK(pci_bar_size(0xffffffffffe00004ULL, ~0x0fULL, 1)
          == 0x00200000);
    CHECK(pci_bar_size(0xffffffffffd00004ULL, ~0x0fULL, 1) == 0);

    const u16 vendor = PCI_VENDOR_ID_CIRRUS;
    const u16 device = PCI_DEVICE_ID_CIRRUS_5446;
    const u16 class = PCI_CLASS_DISPLAY_VGA;
    CHECK(cirrus_pci_framebuffer(vendor, device, class, 0, DECODE,
                                 0xe0000008U) == 0xe0000000U);
    CHECK(cirrus_pci_framebuffer(vendor, device, class, 0x80, DECODE,
                                 0xe1000000U) == 0xe1000000U);
    CHECK(cirrus_pci_framebuffer(vendor, PCI_DEVICE_ID_CIRRUS_5430,
                                 class, 0, DECODE, 0xe2000008U)
          == 0xe2000000U);
    CHECK(!cirrus_pci_framebuffer(vendor, device, class, 0,
                                  PCI_COMMAND_IO, 0xe0000000U));
    CHECK(!cirrus_pci_framebuffer(vendor, device, class, 0, DECODE, 0));
    CHECK(!cirrus_pci_framebuffer(vendor, device, class, 0, DECODE, ~0U));
    CHECK(!cirrus_pci_framebuffer(vendor, device, class, 0, DECODE,
                                  0xe0000001U));
    CHECK(!cirrus_pci_framebuffer(vendor, device, class, 0, DECODE,
                                  0xe0000004U));
    CHECK(!cirrus_pci_framebuffer(vendor, device, class, 0, DECODE,
                                  0xe0100000U));
    CHECK(!cirrus_pci_framebuffer(0xffff, device, class, 0, DECODE,
                                  0xe0000000U));
    CHECK(!cirrus_pci_framebuffer(vendor, device, 0x0604, 0, DECODE,
                                  0xe0000000U));
    CHECK(!cirrus_pci_framebuffer(vendor, device, class, 1, DECODE,
                                  0xe0000000U));
    return 0;
}
