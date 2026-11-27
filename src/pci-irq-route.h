// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __PCI_IRQ_ROUTE_H
#define __PCI_IRQ_ROUTE_H

// Intel PIIX3/4 PIRQRC and ELCR: IRQ 3-7, 9-12, 14-15.
#define PCI_PIIX_IRQ_BITMAP 0xdef8U
#define PCI_PIIX_ROUTER_BDF 0x0008
#define PCI_PIIX3_ROUTER_ID 0x70008086U
#define PCI_PIIX4_ROUTER_ID 0x71108086U

static inline int
pci_piix_route_valid(unsigned int pin, unsigned int irq,
                     unsigned int link, unsigned int bitmap)
{
    return pin >= 0x0a && pin <= 0x0d && irq < 16
        && link >= 0x60 && link <= 0x63
        && ((PCI_PIIX_IRQ_BITMAP & bitmap) & (1U << irq));
}

// The caller has validated the routing table and chipset identity.
// Do not clear the old IRQ's ELCR bit: other devices may still use it.
static inline int
pci_piix_route_apply(unsigned int link, unsigned int irq)
{
    unsigned int port = 0x4d0 + (irq >> 3);
    unsigned int old_level = inb(port);
    unsigned int level = old_level | (1U << (irq & 7));
    unsigned int old_route = pci_ioconfig_readb(PCI_PIIX_ROUTER_BDF, link);

    outb(level, port);
    if (!(inb(port) & (1U << (irq & 7)))) {
        outb(old_level, port);
        return -1;
    }
    pci_ioconfig_writeb(PCI_PIIX_ROUTER_BDF, link, irq);
    if (pci_ioconfig_readb(PCI_PIIX_ROUTER_BDF, link) != irq) {
        pci_ioconfig_writeb(PCI_PIIX_ROUTER_BDF, link, old_route);
        outb(old_level, port);
        return -1;
    }
    return 0;
}

#endif
