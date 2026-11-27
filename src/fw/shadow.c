// Support for enabling/disabling BIOS ram shadowing.
//
// Copyright (C) 2008-2010  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2006 Fabrice Bellard
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "config.h" // CONFIG_*
#include "dev-q35.h" // PCI_VENDOR_ID_INTEL
#include "dev-piix.h" // I440FX_PAM0
#include "hw/pci.h" // pci_config_writeb
#include "hw/pci_ids.h" // PCI_VENDOR_ID_INTEL
#include "hw/pci_regs.h" // PCI_VENDOR_ID
#include "malloc.h" // rom_get_last
#include "output.h" // dprintf
#include "paravirt.h" // runningOnXen
#include "shadow-umb.h" // shadow_umb_pam
#include "string.h" // memset
#include "util.h" // make_bios_writable
#include "x86.h" // wbinvd

// On the emulators, the bios at 0xf0000 is also at 0xffff0000
#define BIOS_SRC_OFFSET 0xfff00000

// Enable shadowing and copy only the ROM-backed parts of the image.
// Execute from the high ROM alias if any firmware block changes its
// read mapping. This also works on machines without independent PAM reads.
static void noinline
__make_bios_writable_intel(u32 bdf_mask, u32 pam0)
{
    u16 bdf = bdf_mask;
    u16 copy_mask = bdf_mask >> 16;

    // Write only the PAM bytes, not adjacent chipset registers.
    for (int i = 1; i < 7; i++)
        pci_ioconfig_writeb(bdf, pam0 + i, 0x33);
    pci_ioconfig_writeb(bdf, pam0, 0x30);

    u32 image_start = SYMBOL(code32flat_start);
    u32 image_end = SYMBOL(code32flat_end);
    for (unsigned i = 0; copy_mask && i < 16; i++) {
        if (!(copy_mask & (1u << i)))
            continue;
        u32 start = SHADOW_WRITE_BASE + i * SHADOW_UMB_BLOCK;
        u32 end = start + SHADOW_UMB_BLOCK;
        if (start < image_start)
            start = image_start;
        if (end > image_end)
            end = image_end;
        memcpy((void*)start, (void*)(start + BIOS_SRC_OFFSET), end - start);
    }
}

static void
make_bios_writable_intel(u16 bdf, u32 pam0)
{
    u8 pam[7];
    for (unsigned i = 0; i < ARRAY_SIZE(pam); i++)
        pam[i] = pci_ioconfig_readb(bdf, pam0 + i);

    u16 copy_mask = shadow_copy_mask(pam, SYMBOL(code32flat_start),
                                    SYMBOL(code32flat_end));
    if (copy_mask) {
        // Do not execute from any low ROM block while changing its mapping.
        u32 pos = (u32)__make_bios_writable_intel + BIOS_SRC_OFFSET;
        // Leave one regparm register free for the indirect call target.
        u32 bdf_mask = bdf | ((u32)copy_mask << 16);
        void (*func)(u32 bdf_mask, u32 pam0) = (void*)pos;
        func(bdf_mask, pam0);
        return;
    }
    // All firmware blocks already read from RAM; just enable writes.
    __make_bios_writable_intel(bdf, pam0);
}

static void
make_bios_readonly_intel(u16 bdf, u32 pam0)
{
    // Flush any pending writes before locking memory.
    wbinvd();

    // Classify every 16 KiB block, including the final partial
    // ROM/runtime boundary. A larger BIOS may contain code below F0000h.
    u32 romlast = rom_get_last();
    u32 readonly_start = SYMBOL(final_readonly_start);
    u32 runtime_start = rom_get_max();
    int i;
    for (i = 0; i < 6; i++) {
        u32 mem = BUILD_ROM_START + i * 2 * SHADOW_UMB_BLOCK;
        pci_config_writeb(bdf, pam0 + i + 1,
                          shadow_umb_pam(mem, romlast, runtime_start,
                                         readonly_start,
                                         CONFIG_MALLOC_UPPERMEMORY,
                                         CONFIG_WRITABLE_UPPERMEMORY));
    }

    // Write protect 0xf0000-0x100000
    pci_config_writeb(bdf, pam0, 0x10);
}

static int ShadowBDF = -1;

// Make the 0xc0000-0x100000 area read/writable.
void
make_bios_writable(void)
{
    if (!CONFIG_QEMU || runningOnXen())
        return;

    dprintf(3, "enabling shadow ram\n");

    // At this point, statically allocated variables can't be written,
    // so do this search manually.
    int bdf;
    pci_ioconfig_foreachbdf(bdf, 0) {
        u32 vendev = pci_ioconfig_readl(bdf, PCI_VENDOR_ID);
        u16 vendor = vendev & 0xffff, device = vendev >> 16;
        if (vendor == PCI_VENDOR_ID_INTEL
            && (device == PCI_DEVICE_ID_INTEL_82441
                || device == PCI_DEVICE_ID_INTEL_82443BX_2)) {
            make_bios_writable_intel(bdf, I440FX_PAM0);
            code_mutable_preinit();
            ShadowBDF = bdf;
            return;
        }
        if (vendor == PCI_VENDOR_ID_INTEL
            && device == PCI_DEVICE_ID_INTEL_Q35_MCH) {
            make_bios_writable_intel(bdf, Q35_HOST_BRIDGE_PAM0);
            code_mutable_preinit();
            ShadowBDF = bdf;
            return;
        }
    }
    dprintf(1, "Unable to unlock ram - bridge not found\n");
}

// Make the BIOS code segment area (0xf0000) read-only.
void
make_bios_readonly(void)
{
    if (!CONFIG_QEMU || runningOnXen())
        return;
    dprintf(3, "locking shadow ram\n");

    if (ShadowBDF < 0) {
        dprintf(1, "Unable to lock ram - bridge not found\n");
        return;
    }

    u16 device = pci_config_readw(ShadowBDF, PCI_DEVICE_ID);
    if (device == PCI_DEVICE_ID_INTEL_82441
        || device == PCI_DEVICE_ID_INTEL_82443BX_2)
        make_bios_readonly_intel(ShadowBDF, I440FX_PAM0);
    else
        make_bios_readonly_intel(ShadowBDF, Q35_HOST_BRIDGE_PAM0);
}

void
qemu_reboot(void)
{
    if (!CONFIG_QEMU || runningOnXen())
        return;
    // QEMU doesn't map 0xc0000-0xfffff back to the original rom on a
    // reset, so do that manually before invoking a hard reset.
    void *flash = (void*)BIOS_SRC_OFFSET;
    u32 hrp = (u32)&HaveRunPost;
    if (readl(flash + hrp)) {
        // There isn't a pristine copy of the BIOS at 0xffff0000 to copy
        if (HaveRunPost == 3) {
            // In a reboot loop.  Try to shutdown the machine instead.
            dprintf(1, "Unable to hard-reboot machine - attempting shutdown.\n");
            apm_shutdown();
        }
        make_bios_writable();
        HaveRunPost = 3;
    } else {
        // Copy the BIOS making sure to only reset HaveRunPost at end
        make_bios_writable();
        u32 cstart = SYMBOL(code32flat_start), cend = SYMBOL(code32flat_end);
        memcpy((void*)cstart, flash + cstart, hrp - cstart);
        memcpy((void*)hrp + 4, flash + hrp + 4, cend - (hrp + 4));
        barrier();
        HaveRunPost = 0;
        barrier();
    }

    // Request a QEMU system reset.  Do the reset in this function as
    // the BIOS code was overwritten above and not all BIOS
    // functionality may be available.

    // Attempt PCI style reset
    outb(0x02, PORT_PCI_REBOOT);
    outb(0x06, PORT_PCI_REBOOT);

    // Next try triple faulting the CPU to force a reset
    asm volatile("int3");
}
