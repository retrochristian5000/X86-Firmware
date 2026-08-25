// Multiboot interface support.
//
// Copyright (C) 2015  Vladimir Serbinenko <phcoder@gmail.com>
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "config.h" // CONFIG_*
#include "e820map.h" // e820_add
#include "malloc.h" // free
#include "output.h" // dprintf
#include "romfile.h" // romfile_add
#include "std/multiboot.h" // MULTIBOOT_*
#include "string.h" // memset
#include "util.h" // multiboot_init

struct mbfs_romfile_s {
    struct romfile_s file;
    void *data;
};

u32 __VISIBLE entry_elf_eax, entry_elf_ebx;
static int MultibootMemoryMap;

static struct multiboot_info *
get_multiboot_info(void)
{
    if (!CONFIG_MULTIBOOT || entry_elf_eax != MULTIBOOT_BOOTLOADER_MAGIC
        || !entry_elf_ebx)
        return NULL;
    return (void *)entry_elf_ebx;
}

static u32
multiboot_e820_type(u32 type)
{
    switch (type) {
    case MULTIBOOT_MEMORY_AVAILABLE:
        return E820_RAM;
    case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
        return E820_ACPI;
    case MULTIBOOT_MEMORY_NVS:
        return E820_NVS;
    case MULTIBOOT_MEMORY_BADRAM:
        return E820_UNUSABLE;
    case MULTIBOOT_MEMORY_RESERVED:
    default:
        return E820_RESERVED;
    }
}

static void
multiboot_reserve_string(u32 addr)
{
    if (addr)
        e820_add(addr, strlen((char *)addr) + 1, E820_RESERVED);
}

int
multiboot_has_memory_map(void)
{
    return MultibootMemoryMap;
}

// Import the bootloader memory map before malloc_preinit().  A GRUB EFI launch
// does not provide a coreboot table, and the legacy 16MiB coreboot fallback is
// too small for reliable controller queues and DMA buffers on real hardware.
void
multiboot_preinit(void)
{
    struct multiboot_info *mbi = get_multiboot_info();
    if (!mbi || !(mbi->flags & MULTIBOOT_INFO_MEM_MAP)
        || !mbi->mmap_addr || !mbi->mmap_length)
        return;

    u32 pos = mbi->mmap_addr;
    u32 end = pos + mbi->mmap_length;
    if (end < pos)
        return;

    // Validate the complete variable-length map before modifying e820_list.
    u32 check = pos;
    int count = 0;
    while (check < end) {
        if (end - check < sizeof(u32))
            return;
        struct multiboot_mmap_entry *entry = (void *)check;
        u32 payload = entry->size;
        u32 minimum = sizeof(*entry) - sizeof(entry->size);
        if (payload < minimum)
            return;
        u32 total = payload + sizeof(entry->size);
        if (total < payload || total > end - check)
            return;
        check += total;
        count++;
    }
    if (check != end || !count)
        return;

    while (pos < end) {
        struct multiboot_mmap_entry *entry = (void *)pos;
        u32 total = entry->size + sizeof(entry->size);
        e820_add(entry->addr, entry->len, multiboot_e820_type(entry->type));
        pos += total;
    }

    // Mark the handoff data itself unavailable to SeaBIOS allocations.  GRUB
    // may place these objects inside ranges otherwise reported as available.
    e820_add(entry_elf_ebx, sizeof(*mbi), E820_RESERVED);
    e820_add(mbi->mmap_addr, mbi->mmap_length, E820_RESERVED);

    if (mbi->flags & MULTIBOOT_INFO_CMDLINE)
        multiboot_reserve_string(mbi->cmdline);
    if (mbi->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME)
        multiboot_reserve_string(mbi->boot_loader_name);

    if ((mbi->flags & MULTIBOOT_INFO_MODS) && mbi->mods_addr
        && mbi->mods_count) {
        struct multiboot_mod_list *mods = (void *)mbi->mods_addr;
        if (mbi->mods_count <= (u32)-1 / sizeof(*mods)) {
            e820_add(mbi->mods_addr, mbi->mods_count * sizeof(*mods),
                     E820_RESERVED);
            int i;
            for (i = 0; i < mbi->mods_count; i++) {
                if (mods[i].mod_end > mods[i].mod_start)
                    e820_add(mods[i].mod_start,
                             mods[i].mod_end - mods[i].mod_start,
                             E820_RESERVED);
                multiboot_reserve_string(mods[i].cmdline);
            }
        }
    }

    MultibootMemoryMap = 1;
    dprintf(1, "Using Multiboot memory map for RAM and DMA allocation.\n");
}

static int
extract_filename(char *dest, char *src, size_t lim)
{
    char *ptr;
    for (ptr = src; *ptr; ptr++) {
        if (!(ptr == src || ptr[-1] == ' ' || ptr[-1] == '\t'))
            continue;
        /* memcmp stops early if it encounters \0 as it doesn't match name=.  */
        if (memcmp(ptr, "name=", 5) == 0) {
            int i;
            char *optr = dest;
            for (i = 0, ptr += 5; *ptr && *ptr != ' ' && i < lim; i++) {
                *optr++ = *ptr++;
            }
            *optr++ = '\0';
            return 1;
        }
    }
    return 0;
}

// Copy a file to memory
static int
mbfs_copyfile(struct romfile_s *file, void *dst, u32 maxlen)
{
    struct mbfs_romfile_s *cfile;
    cfile = container_of(file, struct mbfs_romfile_s, file);
    u32 size = cfile->file.size;
    void *src = cfile->data;

    // Not compressed.
    dprintf(3, "Copying data %d@%p to %d@%p\n", size, src, maxlen, dst);
    if (size > maxlen) {
        warn_noalloc();
        return -1;
    }
    iomemcpy(dst, src, size);
    return size;
}

void
multiboot_init(void)
{
    struct multiboot_info *mbi = get_multiboot_info();
    if (!mbi)
        return;
    dprintf(1, "multiboot: eax=%x, ebx=%x\n", entry_elf_eax, entry_elf_ebx);
    dprintf(1, "mbptr=%p\n", mbi);
    dprintf(1, "flags=0x%x, mods=0x%x, mods_c=%d\n", mbi->flags, mbi->mods_addr,
            mbi->mods_count);
    if (!(mbi->flags & MULTIBOOT_INFO_MODS))
        return;
    int i;
    struct multiboot_mod_list *mod = (void *)mbi->mods_addr;
    for (i = 0; i < mbi->mods_count; i++) {
        struct mbfs_romfile_s *cfile;
        u8 *copy;
        u32 len;
        if (!mod[i].cmdline)
            continue;
        len = mod[i].mod_end - mod[i].mod_start;
        cfile = malloc_tmp(sizeof(*cfile));
        if (!cfile) {
            warn_noalloc();
            return;
        }
        memset(cfile, 0, sizeof(*cfile));
        dprintf(1, "module %s, size 0x%x\n", (char *)mod[i].cmdline, len);
        if (!extract_filename(cfile->file.name, (char *)mod[i].cmdline,
                              sizeof(cfile->file.name))) {
            free(cfile);
            continue;
        }
        dprintf(1, "assigned file name <%s>\n", cfile->file.name);
        cfile->file.size = len;
        copy = malloc_tmp(len);
        if (!copy) {
            warn_noalloc();
            free(cfile);
            return;
        }
        memcpy(copy, (void *)mod[i].mod_start, len);
        cfile->file.copy = mbfs_copyfile;
        cfile->data = copy;
        romfile_add(&cfile->file);
    }
}
