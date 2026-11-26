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
#include "util.h" // PirAddr

struct pir_table {
    struct pir_header pir;
    struct pir_slot slots[6];
} PACKED;

static struct pir_table PIR_TABLE = {
    .pir = {
        .version = 0x0100,
        .size = sizeof(struct pir_table),
    },
    .slots = {
        {
            // first slot entry PCI-to-ISA (embedded)
            .dev = 1<<3,
            .links = {
                {.link = 0x60, .bitmap = 0xdef8}, // INTA#
                {.link = 0x61, .bitmap = 0xdef8}, // INTB#
                {.link = 0x62, .bitmap = 0xdef8}, // INTC#
                {.link = 0x63, .bitmap = 0xdef8}, // INTD#
            },
            .slot_nr = 0, // embedded
        }, {
            // second slot entry: 1st PCI slot
            .dev = 2<<3,
            .links = {
                {.link = 0x61, .bitmap = 0xdef8}, // INTA#
                {.link = 0x62, .bitmap = 0xdef8}, // INTB#
                {.link = 0x63, .bitmap = 0xdef8}, // INTC#
                {.link = 0x60, .bitmap = 0xdef8}, // INTD#
            },
            .slot_nr = 1,
        }, {
            // third slot entry: 2nd PCI slot
            .dev = 3<<3,
            .links = {
                {.link = 0x62, .bitmap = 0xdef8}, // INTA#
                {.link = 0x63, .bitmap = 0xdef8}, // INTB#
                {.link = 0x60, .bitmap = 0xdef8}, // INTC#
                {.link = 0x61, .bitmap = 0xdef8}, // INTD#
            },
            .slot_nr = 2,
        }, {
            // 4th slot entry: 3rd PCI slot
            .dev = 4<<3,
            .links = {
                {.link = 0x63, .bitmap = 0xdef8}, // INTA#
                {.link = 0x60, .bitmap = 0xdef8}, // INTB#
                {.link = 0x61, .bitmap = 0xdef8}, // INTC#
                {.link = 0x62, .bitmap = 0xdef8}, // INTD#
            },
            .slot_nr = 3,
        }, {
            // 5th slot entry: 4th PCI slot
            .dev = 5<<3,
            .links = {
                {.link = 0x60, .bitmap = 0xdef8}, // INTA#
                {.link = 0x61, .bitmap = 0xdef8}, // INTB#
                {.link = 0x62, .bitmap = 0xdef8}, // INTC#
                {.link = 0x63, .bitmap = 0xdef8}, // INTD#
            },
            .slot_nr = 4,
        }, {
            // 6th slot entry: 5th PCI slot
            .dev = 6<<3,
            .links = {
                {.link = 0x61, .bitmap = 0xdef8}, // INTA#
                {.link = 0x62, .bitmap = 0xdef8}, // INTB#
                {.link = 0x63, .bitmap = 0xdef8}, // INTC#
                {.link = 0x60, .bitmap = 0xdef8}, // INTD#
            },
            .slot_nr = 5,
        },
    }
};

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

    PIR_TABLE.pir.router_bus = pci_bdf_to_bus(router->bdf);
    PIR_TABLE.pir.router_devfunc = pci_bdf_to_devfn(router->bdf);
    PIR_TABLE.pir.compatible_devid = ((u32)router->device << 16)
                                    | router->vendor;
    PIR_TABLE.pir.signature = PIR_SIGNATURE;
    PIR_TABLE.pir.checksum -= checksum(&PIR_TABLE, sizeof(PIR_TABLE));
    copy_pir(&PIR_TABLE);
}
