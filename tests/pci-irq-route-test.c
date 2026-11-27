// SPDX-License-Identifier: LGPL-3.0-or-later
#include <assert.h>
#include <stdint.h>

static uint8_t elcr[2], routes[4];
static unsigned writes;
static int reject_elcr, reject_route;

static uint8_t inb(unsigned port)
{
    assert(port == 0x4d0 || port == 0x4d1);
    return elcr[port - 0x4d0];
}

static void outb(unsigned value, unsigned port)
{
    assert(port == 0x4d0 || port == 0x4d1);
    writes++;
    if (!reject_elcr)
        elcr[port - 0x4d0] = value;
}

static uint8_t pci_ioconfig_readb(unsigned bdf, unsigned link)
{
    assert(bdf == 8 && link >= 0x60 && link <= 0x63);
    return routes[link - 0x60];
}

static void pci_ioconfig_writeb(unsigned bdf, unsigned link, unsigned value)
{
    assert(bdf == 8 && link >= 0x60 && link <= 0x63);
    writes++;
    if (!reject_route)
        routes[link - 0x60] = value;
}

#include "../src/pci-irq-route.h"

int main(void)
{
    for (unsigned pin = 0; pin < 256; pin++)
        for (unsigned irq = 0; irq < 256; irq++)
            for (unsigned link = 0x5f; link <= 0x64; link++) {
                int valid = pin >= 10 && pin <= 13 && irq < 16
                    && link >= 0x60 && link <= 0x63
                    && (0xdef8U & (1U << irq));
                assert(!!pci_piix_route_valid(pin, irq, link, 0xffff)
                       == !!valid);
            }
    assert(!pci_piix_route_valid(10, 5, 0x60, 0x0c00));
    assert(!pci_piix_route_valid(10, 10, 0x60, 0));
    assert(pci_piix_route_valid(13, 10, 0x63, 0x0400));
    assert(writes == 0);

    // Keep other PIRQ routes and existing ELCR bits intact.
    routes[0] = 10;
    routes[1] = 11;
    elcr[0] = 8;
    elcr[1] = 12;
    assert(pci_piix_route_apply(0x60, 5) == 0);
    assert(routes[0] == 5 && routes[1] == 11);
    assert(elcr[0] == 0x28 && elcr[1] == 12);
    assert(writes == 2);

    // A refused route write must restore the prior trigger state.
    reject_route = 1;
    assert(pci_piix_route_apply(0x60, 9) == -1);
    assert(routes[0] == 5 && elcr[1] == 12);
    reject_route = 0;

    // A refused ELCR write must not change the route.
    reject_elcr = 1;
    assert(pci_piix_route_apply(0x60, 7) == -1);
    assert(routes[0] == 5 && elcr[0] == 0x28);
    return 0;
}
