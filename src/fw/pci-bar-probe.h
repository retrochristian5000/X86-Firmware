// This file may be distributed under the terms of the GNU LGPLv3 license.
#ifndef __PCI_BAR_PROBE_H
#define __PCI_BAR_PROBE_H

#include "hw/pci.h"
#include "hw/pci_regs.h"

// Preserve all unrelated command bits; never write the adjacent Status word.
static inline u16
pci_bar_disable_decode(u16 bdf)
{
    u16 cmd = pci_config_readw(bdf, PCI_COMMAND);
    u16 disabled = cmd & ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
    if (disabled != cmd)
        pci_config_writew(bdf, PCI_COMMAND, disabled);
    return cmd;
}

static inline void
pci_bar_restore_decode(u16 bdf, u16 cmd)
{
    if (cmd & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY))
        pci_config_writew(bdf, PCI_COMMAND, cmd);
}

// Probe both halves of a 64-bit BAR before reading either mask.
static inline u64
pci_bar_probe_mask(u16 bdf, u32 ofs, u32 probe, int is64)
{
    u16 cmd = pci_bar_disable_decode(bdf);
    u32 lo = pci_config_readl(bdf, ofs);
    u32 hi = is64 ? pci_config_readl(bdf, ofs + 4) : 0;

    pci_config_writel(bdf, ofs, probe);
    if (is64)
        pci_config_writel(bdf, ofs + 4, ~0U);
    u64 mask = pci_config_readl(bdf, ofs);
    if (is64)
        mask |= (u64)pci_config_readl(bdf, ofs + 4) << 32;

    if (is64)
        pci_config_writel(bdf, ofs + 4, hi);
    pci_config_writel(bdf, ofs, lo);
    pci_bar_restore_decode(bdf, cmd);
    return mask;
}

static inline void
pci_bar_assign(u16 bdf, u32 ofs, u64 addr, int is64)
{
    u16 cmd = pci_bar_disable_decode(bdf);
    if (is64)
        pci_config_writel(bdf, ofs + 4, addr >> 32);
    pci_config_writel(bdf, ofs, addr);
    pci_bar_restore_decode(bdf, cmd);
}

#endif
