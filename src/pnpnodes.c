// Experimental PnP BIOS system-device enumeration.
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "config.h"
#include "farptr.h"
#include "hw/pci.h"
#include "hw/pcidevice.h"
#include "hw/pci_ids.h"
#include "pnpnodes.h"
#include "romfile.h"
#include "std/pnpbios.h"
#include "string.h"

#if CONFIG_PNPBIOS_RUNTIME
#define PNP_NODE_STORAGE VARFSEG
#include "pnp-fixed-nodes.h"
u8 PnpNodeMask VARFSEG;

// PNP0A03: PCI bus, type 06h/04h/00h. Fixed, non-disableable.
// Only conventional PCI configuration I/O is claimed. Do not invent
// PCI apertures, IRQ assignments or motherboard resource windows.
u8 PnpPciNode[] VARFSEG = {
    0x1a, 0x00,       // Node size (26 bytes)
    0x00,             // Handle
    0x41, 0xd0, 0x0a, 0x03, // Compressed PNP0A03
    0x06, 0x04, 0x00, // Device type
    0x03, 0x00,       // Cannot disable or configure
    0x47, 0x01, 0xf8, 0x0c, 0xf8, 0x0c, 0x01, 0x08,
                       // 16-bit decode, CF8h-CFFh
    0x79, 0x00,       // Allocated resource end tag
    0x79, 0x00,       // Possible resource end tag
    0x79, 0x00,       // Compatible identifier end tag
};

// PNP0A00: ISA bus. The PIIX bridge supplies the bus; its child devices
// own their individual resources. Do not claim the whole ISA I/O space.
u8 PnpIsaNode[] VARFSEG = {
    0x12, 0x00,       // Node size (18 bytes)
    0x01,             // Stable handle
    0x41, 0xd0, 0x0a, 0x00, // Compressed PNP0A00
    0x06, 0x01, 0x00, // ISA bridge
    0x03, 0x00,       // Cannot disable or configure
    0x79, 0x00,       // Allocated resources
    0x79, 0x00,       // Possible resources
    0x79, 0x00,       // Compatible identifiers
};

// PnP BIOS 1.0A section 4.7.3: revision 1, zero assigned CSNs.
// No ISA PnP isolation has been performed; the read port is invalid.
u8 PnpIsaConfig[6] VARFSEG = { 1, 0, 0, 0, 0, 0 };
#endif

int
pnp_nodes_setup(int acpi_present)
{
#if CONFIG_PNPBIOS_RUNTIME && !MODESEGMENT
    PnpNodeMask = 0;
    if (acpi_present)
        return 0;

    struct pci_device *router =
        pci_find_device(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371SB_0);
    if (!router)
        router = pci_find_device(PCI_VENDOR_ID_INTEL,
                                 PCI_DEVICE_ID_INTEL_82371AB_0);
    if (!router || router->bdf != pci_to_bdf(0, 1, 0))
        return 0;
    struct romfile_s *file = romfile_find(PNP_CORE_FILE);
    u64 reported = file && file->size == 1
        ? romfile_loadint(PNP_CORE_FILE, 0xff) : 0xff;
    PnpNodeMask = pnp_core_node_mask(reported);
    return 1;
#else
    return 0;
#endif
}

u16
pnp_get_node_count(u16 *args)
{
#if CONFIG_PNPBIOS_RUNTIME && MODE16
    // Function, NumNodes (far byte pointer), NodeSize (far word
    // pointer), BIOS data segment or selector.
    u8 mask = GET_FARVAR(args[5], PnpNodeMask);
    if (!mask)
        return FUNCTION_NOT_SUPPORTED;
    if (args[3] == 0xffff)
        return PNP_BAD_PARAMETER;

    u8 count = 0;
    u16 largest = 0;
    for (unsigned int node = 0; node < PNP_HANDLE_LIMIT; node++) {
        if (!(mask & (1U << node)))
            continue;
        count++;
        u16 size = pnp_core_node_size(node);
        if (size > largest)
            largest = size;
    }
    SET_FARVAR(args[2], *(u8 *)(args[1]+0), count);
    SET_FARVAR(args[4], *(u16 *)(args[3]+0), largest);
    return 0;
#else
    return FUNCTION_NOT_SUPPORTED;
#endif
}

u16
pnp_get_node(u16 *args)
{
#if CONFIG_PNPBIOS_RUNTIME && MODE16
    // Function, Node (far byte pointer), Buffer (far pointer),
    // Control, BIOS data segment or selector.
    u8 mask = GET_FARVAR(args[6], PnpNodeMask);
    if (!mask)
        return FUNCTION_NOT_SUPPORTED;
    if (args[5] != 1 && args[5] != 2)
        return PNP_BAD_PARAMETER;
    u8 node = GET_FARVAR(args[2], *(u8 *)(args[1]+0));
    if (node >= PNP_HANDLE_LIMIT || !(mask & (1U << node)))
        return PNP_INVALID_HANDLE;
    u8 *data;
    switch (node) {
    case 0: data = PnpPciNode; break;
    case 1: data = PnpIsaNode; break;
    case 2: data = PnpPicNode; break;
    case 3: data = PnpDmaNode; break;
    case 4: data = PnpPitNode; break;
    case 5: data = PnpRtcNode; break;
    default: return PNP_INVALID_HANDLE;
    }
    u16 size = pnp_core_node_size(node);
    if ((u32)args[3] + size > 0x10000)
        return PNP_BUFFER_TOO_SMALL;

    // Use the caller's BIOS data selector in both execution modes.
    // The firmware nodes are immutable once POST is complete.
    memcpy_far(args[4], (void *)(args[3]+0), args[6], data, size);
    u8 next = pnp_next_node(mask, node);
    SET_FARVAR(args[2], *(u8 *)(args[1]+0), next);
    return 0;
#else
    return FUNCTION_NOT_SUPPORTED;
#endif
}

u16
pnp_set_node(u16 *args)
{
#if CONFIG_PNPBIOS_RUNTIME && MODE16
    // Function, Node (by value), Buffer (far pointer), Control,
    // BIOS data segment or selector.
    u8 mask = GET_FARVAR(args[5], PnpNodeMask);
    if (!mask)
        return FUNCTION_NOT_SUPPORTED;
    if (!args[4] || args[4] > 3)
        return PNP_BAD_PARAMETER;
    if (args[1] >= PNP_HANDLE_LIMIT || !(mask & (1U << args[1])))
        return PNP_INVALID_HANDLE;

    // Fixed system nodes cannot be relocated or disabled.
    return PNP_SET_FAILED;
#else
    return FUNCTION_NOT_SUPPORTED;
#endif
}

u16
pnp_get_isa_config(u16 *args)
{
#if CONFIG_PNPBIOS_RUNTIME && MODE16
    // Function, Configuration (far pointer), BIOS data segment/selector.
    if (!(GET_FARVAR(args[3], PnpNodeMask) & 2))
        return FUNCTION_NOT_SUPPORTED;
    if ((u32)args[1] + sizeof(PnpIsaConfig) > 0x10000)
        return PNP_BUFFER_TOO_SMALL;

    memcpy_far(args[2], (void *)(args[1]+0), args[3],
               PnpIsaConfig, sizeof(PnpIsaConfig));
    return 0;
#else
    return FUNCTION_NOT_SUPPORTED;
#endif
}
