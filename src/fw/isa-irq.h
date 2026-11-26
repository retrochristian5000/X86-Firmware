// PIIX boot IRQ policy shared by PCI setup and the legacy routing table.
// This file may be distributed under the terms of the GNU LGPLv3 license.
#ifndef __FW_ISA_IRQ_H
#define __FW_ISA_IRQ_H

#define ISA_IRQ_MASK_FILE "etc/isa-reserved-irq-mask"
#define ISA_IRQ_LEGACY_FILE "etc/isa-ne2000-irq-mask"
#define ISA_IRQ_MASK_MISSING 0x10000U
#define PIIX_PIRQ_LEGACY_BITMAP 0xdef8U
#define PIIX_PIRQ_MANAGED_BITMAP 0x0c20U // IRQ 5, 10, 11

// The new mask is a 16-bit boot-time snapshot, not a live PnP inventory.
// Retain the old NE2000-only mask when booting with older QEMU.
static inline unsigned int
piix_reserved_irqs(unsigned long long reported, unsigned int legacy)
{
    return reported <= 0xffffU ? (unsigned int)reported | legacy : legacy;
}

static inline unsigned int
piix_pirq_options(unsigned long long reported, unsigned int legacy)
{
    unsigned int candidates = reported <= 0xffffU
        ? PIIX_PIRQ_MANAGED_BITMAP : PIIX_PIRQ_LEGACY_BITMAP;
    return candidates & ~piix_reserved_irqs(reported, legacy);
}

// Keep the existing boot preference for IRQ10/11. IRQ5 is a possible
// OS-selected route but is not a new automatic firmware fallback.
static inline unsigned int
piix_pirq_select(unsigned int preferred, unsigned int reserved)
{
    if (!(reserved & (1U << preferred)))
        return preferred;
    unsigned int alternate = preferred == 10 ? 11 : 10;
    return reserved & (1U << alternate) ? 0x80 : alternate;
}

#endif
