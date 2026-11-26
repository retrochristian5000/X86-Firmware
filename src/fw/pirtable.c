// PIR table generation (for emulators)
// DO NOT ADD NEW FEATURES HERE.  (See paravirt.c / biostables.c instead.)
//
// Copyright (C) 2008  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2002  MandrakeSoft S.A.
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "config.h" // CONFIG_*
#include "hw/pci.h" // pci_bdf_to_bus
#include "hw/pcidevice.h" // pci_find_device
#include "hw/pci_ids.h" // PCI_DEVICE_ID_INTEL_82371SB_0
#include "output.h" // dprintf
#include "std/pirtable.h" // struct pir_header
#include "string.h" // checksum
#include "romfile.h" // romfile_loadint
#include "isa-irq.h" // piix_pirq_options
#include "util.h" // PirAddr

// QEMU i440fx has 32 root-bus device numbers; slot zero is the host
// bridge. Retain the original six entries, and describe additional
// occupied root-bus slots without reserving F-segment space for empty ones.
#define PIR_ROOT_SLOTS 31
#define PIR_LEGACY_SLOTS 6

struct pir_table {
    struct pir_header pir;
    struct pir_slot slots[PIR_ROOT_SLOTS];
} PACKED;

static void
pir_add_slot(struct pir_table *table, int dev, u16 irq_bitmap)
{
    struct pir_slot *slot = &table->slots[
        (table->pir.size - sizeof(table->pir)) / sizeof(*slot)];
    slot->dev = dev << 3;
    // QEMU pc_pci_slot_get_pirq(): rotate INTA-D by device number minus one.
    for (int pin = 0; pin < 4; pin++) {
        slot->links[pin].link = 0x60 + ((dev - 1 + pin) & 3);
        slot->links[pin].bitmap = irq_bitmap;
    }
    slot->slot_nr = dev - 1;
    table->pir.size += sizeof(*slot);
}

void
pirtable_setup(void)
{
    if (! CONFIG_PIRTABLE)
        return;

    // This legacy table describes PIIX root-bus routing only. Never
    // advertise a PIIX router on Q35 or an unrelated chipset.
    struct pci_device *router =
        pci_find_device(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371SB_0);
    if (!router)
        router = pci_find_device(PCI_VENDOR_ID_INTEL,
                                 PCI_DEVICE_ID_INTEL_82371AB_0);
    if (!router || router->bdf != pci_to_bdf(0, 1, 0))
        return;

    dprintf(3, "init PIIX PIR table\n");

    struct pir_table table;
    memset(&table, 0, sizeof(table));
    table.pir.version = 0x0100;
    table.pir.size = sizeof(table.pir);
    table.pir.router_bus = pci_bdf_to_bus(router->bdf);
    table.pir.router_devfunc = pci_bdf_to_devfn(router->bdf);
    table.pir.compatible_devid = ((u32)router->device << 16)
                                | router->vendor;
    table.pir.signature = PIR_SIGNATURE;

    u32 occupied = 0;
    struct pci_device *pci;
    foreachpci(pci) {
        if (!pci_bdf_to_bus(pci->bdf))
            occupied |= 1U << pci_bdf_to_dev(pci->bdf);
    }

    u64 reported = romfile_loadint(ISA_IRQ_MASK_FILE, ISA_IRQ_MASK_MISSING);
    u16 legacy = romfile_loadint(ISA_IRQ_LEGACY_FILE, 0);
    u16 irq_bitmap = piix_pirq_options(reported, legacy);
    for (int dev = 1; dev <= PIR_ROOT_SLOTS; dev++) {
        if (dev <= PIR_LEGACY_SLOTS || (occupied & (1U << dev)))
            pir_add_slot(&table, dev, irq_bitmap);
    }

    table.pir.checksum = -checksum(&table, table.pir.size);
    copy_pir(&table);
}
