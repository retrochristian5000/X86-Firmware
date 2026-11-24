// 32bit code to Power On Self Test (POST) a machine.
//
// Copyright (C) 2008-2013  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2002  MandrakeSoft S.A.
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "biosvar.h" // SET_BDA
#include "block.h" // block_setup
#include "bregs.h" // struct bregs
#include "config.h" // CONFIG_*
#include "e820map.h" // e820_add
#include "fw/paravirt.h" // qemu_cfg_preinit
#include "fw/xen.h" // xen_preinit
#include "hw/pic.h" // pic_setup
#include "hw/ps2port.h" // ps2port_setup
#include "hw/rtc.h" // rtc_write
#include "hw/serialio.h" // serial_debug_preinit
#include "hw/usb.h" // usb_setup
#include "malloc.h" // malloc_init
#include "memmap.h" // SYMBOL
#include "output.h" // dprintf
#include "post-selftest.h" // whp_post_e820_valid, whp_post_ram_fill
#include "romfile.h" // romfile_find
#include "string.h" // memset
#include "util.h" // kbd_init
#include "tcgbios.h" // tpm_*


/****************************************************************
 * BIOS initialization and hardware setup
 ****************************************************************/

static void
ivt_init(void)
{
    dprintf(3, "init ivt\n");

    // Initialize all vectors to the default handler.
    int i;
    for (i=0; i<256; i++)
        SET_IVT(i, FUNC16(entry_iret_official));

    // Initialize all hw vectors to a default hw handler.
    for (i=BIOS_HWIRQ0_VECTOR; i<BIOS_HWIRQ0_VECTOR+8; i++)
        SET_IVT(i, FUNC16(entry_hwpic1));
    for (i=BIOS_HWIRQ8_VECTOR; i<BIOS_HWIRQ8_VECTOR+8; i++)
        SET_IVT(i, FUNC16(entry_hwpic2));

    // Initialize software handlers.
    SET_IVT(0x02, FUNC16(entry_02));
    SET_IVT(0x05, FUNC16(entry_05));
    SET_IVT(0x10, FUNC16(entry_10));
    SET_IVT(0x11, FUNC16(entry_11));
    SET_IVT(0x12, FUNC16(entry_12));
    SET_IVT(0x13, FUNC16(entry_13_official));
    SET_IVT(0x14, FUNC16(entry_14));
    SET_IVT(0x15, FUNC16(entry_15_official));
    SET_IVT(0x16, FUNC16(entry_16));
    SET_IVT(0x17, FUNC16(entry_17));
    SET_IVT(0x18, FUNC16(entry_18));
    SET_IVT(0x19, FUNC16(entry_19_official));
    SET_IVT(0x1a, FUNC16(entry_1a_official));
    SET_IVT(0x40, FUNC16(entry_40));

    // INT 60h-66h reserved for user interrupt
    for (i=0x60; i<=0x66; i++)
        SET_IVT(i, SEGOFF(0, 0));

    // set vector 0x79 to zero
    // this is used by 'gardian angel' protection system
    SET_IVT(0x79, SEGOFF(0, 0));
}

static void
bda_init(void)
{
    dprintf(3, "init bda\n");

    struct bios_data_area_s *bda = MAKE_FLATPTR(SEG_BDA, 0);
    memset(bda, 0, sizeof(*bda));

    int esize = EBDA_SIZE_START;
    u16 ebda_seg = EBDA_SEGMENT_START;
    if (!CONFIG_MALLOC_UPPERMEMORY)
        ebda_seg = FLATPTR_TO_SEG(ALIGN_DOWN(SYMBOL(final_varlow_start), 1024)
                                  - EBDA_SIZE_START*1024);
    SET_BDA(ebda_seg, ebda_seg);

    SET_BDA(mem_size_kb, ebda_seg / (1024/16));

    // Init ebda
    struct extended_bios_data_area_s *ebda = get_ebda_ptr();
    memset(ebda, 0, sizeof(*ebda));
    ebda->size = esize;

    e820_add((u32)ebda, BUILD_LOWRAM_END-(u32)ebda, E820_RESERVED);

    // Init extra stack
    StackPos = &ExtraStack[BUILD_EXTRA_STACK_SIZE] - SYMBOL(zonelow_base);
}

void
interface_init(void)
{
    // Running at new code address - do code relocation fixups
    malloc_init();

    // Setup romfile items.
    qemu_cfg_init();
    coreboot_cbfs_init();
    multiboot_init();

    // Setup ivt/bda/ebda
    ivt_init();
    bda_init();

    // Other interfaces
    boot_init();
    apm_setup();
    bios32_init();
    pmm_init();
    pnp_init();
    kbd_init();
    mouse_init();
}

// Initialize hardware devices
void
device_hardware_setup(void)
{
    usb_setup();
    ps2port_setup();
    block_setup();
    lpt_setup();
    serial_setup();
    cbfs_payload_setup();
}

static void
platform_hardware_setup(void)
{
    // Make sure legacy DMA isn't running.
    dma_setup();

    // Init base pc hardware.  A Multiboot launch from EFI GRUB can inherit a
    // local APIC without the legacy ExtINT virtual wire, so restore that bridge
    // immediately after resetting the 8259 and before interrupts are enabled.
    pic_setup();
    multiboot_setup_legacy_irqs();
    thread_setup();
    mathcp_setup();

    // Platform specific setup
    qemu_platform_setup();
    coreboot_platform_setup();

    // Setup timers and periodic clock interrupt
    timer_setup();
    clock_setup();

    // Initialize TPM
    tpm_setup();
}

void
prepareboot(void)
{
    // Change TPM phys. presence state befor leaving BIOS
    tpm_prepboot();

    // Run BCVs
    bcv_prepboot();

    // Finalize data structures before boot
    cdrom_prepboot();
    pmm_prepboot();
    malloc_prepboot();
    e820_prepboot();

    HaveRunPost = 2;

    // Setup bios checksum.
    BiosChecksum -= checksum((u8*)BUILD_BIOS_ADDR, BUILD_BIOS_SIZE);
}

// Begin the boot process by invoking an int0x19 in 16bit mode.
void VISIBLE32FLAT
startBoot(void)
{
    // Clear low-memory allocations (required by PMM spec).
    memset((void*)BUILD_STACK_ADDR, 0, BUILD_EBDA_MINIMUM - BUILD_STACK_ADDR);

    dprintf(3, "Jump to int19\n");
    struct bregs br;
    memset(&br, 0, sizeof(br));
    br.flags = F_IF;
    call16_int(0x19, &br);
}

// Emit opt-in elapsed POST timings.  The firmware's calibrated timer uses
// ticks per millisecond; IRQ tick conversion is not appropriate here.
static void
post_timing_log(int enabled, u32 *last, const char *phase)
{
    if (!enabled)
        return;
    u32 now = timer_calc(0);
    u32 elapsed = timer_ticks_to_ms(now - *last);
    *last = now;
    dprintf(1, "POST timing: %s: %u ms\n", phase, elapsed);
}

// Optional extended POST: unlike discovery from fw_cfg, these are active
// checks of guest RAM backed by a temporary owned allocation, EBDA/BDA, and
// the E820 map.  A failed check must never fall through into INT 19h.
// This does not claim to test *all* guest RAM, CPUs, or device functions.
static void
whp_extended_post(void)
{
    ASSERT32FLAT();

    if (e820_count <= 0 || e820_count > BUILD_MAX_E820 ||
        whp_post_e820_valid(e820_list, e820_count)) {
        printf("WHP POST FAIL: invalid E820 memory map\n");
        panic("WHP extended POST: E820 map validation failed\n");
    }

    u16 seg = get_ebda_seg();
    u32 ebda_start = (u32)seg << 4;
    u32 conventional_end = (u32)GET_BDA(mem_size_kb) * 1024;
    if (ebda_start < BUILD_EBDA_MINIMUM ||
        ebda_start >= BUILD_LOWRAM_END ||
        conventional_end != ebda_start) {
        printf("WHP POST FAIL: EBDA/BDA memory boundary\n");
        panic("WHP extended POST: EBDA/BDA boundary failed\n");
    }

    u32 ebda_kib = GET_EBDA(seg, size);
    u32 ebda_bytes = ebda_kib * 1024;
    if (!ebda_kib || ebda_bytes > BUILD_LOWRAM_END - ebda_start ||
        !whp_post_e820_reserves(e820_list, e820_count, ebda_start,
                                ebda_bytes)) {
        printf("WHP POST FAIL: EBDA reservation\n");
        panic("WHP extended POST: EBDA reservation failed\n");
    }

    // Test 4 KiB actually allocated from SeaBIOS temporary RAM. Never
    // overwrite arbitrary guest RAM, the IVT, option ROMs, or firmware tables.
    // Volatile accesses make the two address-dependent passes observable.
    const u32 words_count = 4096 / sizeof(u32);
    void *owned = malloc_tmp(4096);
    if (!owned) {
        printf("WHP POST FAIL: no temporary RAM for self-test\n");
        panic("WHP extended POST: temporary RAM allocation failed\n");
    }
    volatile u32 *words = (volatile u32*)owned;
    whp_post_ram_fill(words, words_count, 0);
    int failed = whp_post_ram_verify(words, words_count, 0);
    if (!failed) {
        whp_post_ram_fill(words, words_count, 1);
        failed = whp_post_ram_verify(words, words_count, 1);
    }
    if (!failed)
        failed = whp_post_ram_walk(&words[0]);
    free(owned);
    if (failed) {
        printf("WHP POST FAIL: RAM read/write patterns\n");
        panic("WHP extended POST: RAM pattern validation failed\n");
    }

    printf("WHP extended POST: RAM 4 KiB, E820 and EBDA checks PASS\n");
    dprintf(1, "WHP extended POST: RAM 4 KiB, E820 and EBDA checks PASS\n");
}

// Main setup code.
static void
maininit(void)
{
    // Initialize internal interfaces.
    interface_init();

    // Setup platform devices (including the calibrated firmware timer).
    platform_hardware_setup();

    // Leave normal boots untouched: request timings explicitly through QEMU
    // with -fw_cfg name=opt/org.seabios/whp-post-timing,string=1.  The file's presence,
    // rather than its contents, enables the trace.
    int post_timing = CONFIG_DEBUG_LEVEL
                      && romfile_find("opt/org.seabios/whp-post-timing") != NULL;
    // This QEMU-only report reads existing allocator data after POST; it
    // consumes no extra guest RAM and does not change the E820 memory map.
    int memory_report = CONFIG_DEBUG_LEVEL && CONFIG_QEMU
                        && romfile_find("opt/org.seabios/whp-memory-report") != NULL;
    int extended_post = CONFIG_QEMU
                        && romfile_find("opt/org.seabios/whp-extended-post") != NULL;
    u32 post_last = post_timing ? timer_calc(0) : 0;

    // Keep the normal ordering unless display-first POST is requested.
    // PCI resources, platform timers and TPM setup are already available.
    // With etc/threads=2, defer device discovery until the console is visible
    // but still permit its threads to run during later option ROMs.
    int parallel = threads_during_optionroms();
    int early_display = parallel && CONFIG_QEMU
        && romfile_loadbool("opt/org.seabios/whp-early-display", 0);

    if (parallel && !early_display) {
        device_hardware_setup();
        post_timing_log(post_timing, &post_last, "early device setup");
    }

    // Run VGA option ROM and establish the first usable console.
    vgarom_setup();
    post_timing_log(post_timing, &post_last, "VGA option ROM");
    sercon_setup();
    post_timing_log(post_timing, &post_last, "serial console");
    enable_vga_console();
    post_timing_log(post_timing, &post_last, "VGA console mode / banner");

    // Start deferred hardware discovery exactly once. Only the normal
    // non-preemptive path must wait here before executing option ROMs.
    if (!parallel || early_display)
        device_hardware_setup();
    if (!parallel)
        wait_threads();
    post_timing_log(post_timing, &post_last, "device setup");

    // Run option roms
    optionrom_setup();
    post_timing_log(post_timing, &post_last, "option ROMs");

    // Allow user to modify overall boot order.
    interactive_bootmenu();
    post_timing_log(post_timing, &post_last, "boot menu");
    wait_threads();
    post_timing_log(post_timing, &post_last, "device completion");

    // Test only after parallel device setup and option ROMs have finished,
    // and before prepboot returns temporary memory to the operating system.
    if (extended_post)
        whp_extended_post();
    post_timing_log(post_timing, &post_last, "extended POST");

    // Prepare for boot. malloc_prepboot() has already returned unused
    // whole pages of ZoneHigh to E820 before reporting retained storage.
    prepareboot();
    if (memory_report)
        malloc_report_memory();
    post_timing_log(post_timing, &post_last, "boot preparation");

    // Write protect bios memory.
    make_bios_readonly();

    // Invoke int 19 to start boot process.
    startBoot();
}


/****************************************************************
 * POST entry and code relocation
 ****************************************************************/

// Update given relocs for the code at 'dest' with a given 'delta'
static void
updateRelocs(void *dest, u32 *rstart, u32 *rend, u32 delta)
{
    u32 *reloc;
    for (reloc = rstart; reloc < rend; reloc++)
        *((u32*)(dest + *reloc)) += delta;
}

// Relocate init code and then call a function at its new address.
// The passed function should be in the "init" section and must not
// return.
void __noreturn
reloc_preinit(void *f, void *arg)
{
    void (*func)(void *) __noreturn = f;
    if (!CONFIG_RELOCATE_INIT)
        func(arg);

    // Allocate space for init code.
    u32 initsize = SYMBOL(code32init_end) - SYMBOL(code32init_start);
    u32 codealign = SYMBOL(_reloc_min_align);
    void *codedest = memalign_tmp(codealign, initsize);
    void *codesrc = VSYMBOL(code32init_start);
    if (!codedest)
        panic("No space for init relocation.\n");

    // Copy code and update relocs (init absolute, init relative, and runtime)
    dprintf(1, "Relocating init from %p to %p (size %d)\n"
            , codesrc, codedest, initsize);
    s32 delta = codedest - codesrc;
    memcpy(codedest, codesrc, initsize);
    updateRelocs(codedest, VSYMBOL(_reloc_abs_start), VSYMBOL(_reloc_abs_end)
                 , delta);
    updateRelocs(codedest, VSYMBOL(_reloc_rel_start), VSYMBOL(_reloc_rel_end)
                 , -delta);
    updateRelocs(VSYMBOL(code32flat_start), VSYMBOL(_reloc_init_start)
                 , VSYMBOL(_reloc_init_end), delta);
    if (f >= codesrc && f < VSYMBOL(code32init_end))
        func = f + delta;

    // Call function in relocated code.
    barrier();
    func(arg);
}

// Runs after all code is present and prior to any modifications
void
code_mutable_preinit(void)
{
    if (HaveRunPost)
        // Already run
        return;
    // Setup reset-vector entry point (controls legacy reboots).
    rtc_write(CMOS_RESET_CODE, 0);
    barrier();
    HaveRunPost = 1;
    barrier();
}

// Setup for code relocation and then relocate.
void VISIBLE32INIT
dopost(void)
{
    code_mutable_preinit();

    // Detect ram and setup internal malloc.  Coreboot may install its legacy
    // fallback first; a valid GRUB Multiboot map is authoritative for this
    // launch and replaces it before the allocator or storage DMA setup runs.
    qemu_preinit();
    coreboot_preinit();
    multiboot_preinit();
    malloc_preinit();

    // Relocate initialization code and call maininit().
    reloc_preinit(maininit, NULL);
}

// Entry point for Power On Self Test (POST) - the BIOS initilization
// phase.  This function makes the memory at 0xc0000-0xfffff
// read/writable and then calls dopost().
void VISIBLE32FLAT
handle_post(void)
{
    if (!CONFIG_QEMU && !CONFIG_COREBOOT)
        return;

    serial_debug_preinit();
    debug_banner();

    // Check if we are running under Xen.
    xen_preinit();

    // Allow writes to modify bios area (0xf0000)
    make_bios_writable();

    // Now that memory is read/writable - start post process.
    dopost();
}
