// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef CLEXT_PCI_H
#define CLEXT_PCI_H

#include "types.h"
#include "hw/pci_regs.h"
#include "hw/pci_ids.h"

// PCI10 is a 32-bit memory aperture, not the installed VRAM size.
// The 5446 TRM section 7.4 requires at least 16 MiB alignment;
// revision B's hardware imposes the additional 32 MiB alignment.
// Accept the prefetch bit used by the historical QEMU PCI profile.
static inline u32
cirrus_pci_framebuffer(u16 vendor, u16 device, u16 class, u8 header,
                       u16 command, u32 bar)
{
    if (vendor != PCI_VENDOR_ID_CIRRUS || device == 0xffff
        || class != PCI_CLASS_DISPLAY_VGA
        || (header & 0x7f) != PCI_HEADER_TYPE_NORMAL
        || !(command & PCI_COMMAND_MEMORY)
        || bar == 0xffffffffU
        || (bar & (PCI_BASE_ADDRESS_SPACE_IO | PCI_BASE_ADDRESS_MEM_TYPE_MASK)))
        return 0;

    u32 addr = bar & PCI_BASE_ADDRESS_MEM_MASK;
    if (addr < 0x100000
        || (device == PCI_DEVICE_ID_CIRRUS_5446 && (addr & 0x00ffffff)))
        return 0;
    return addr;
}

#endif
