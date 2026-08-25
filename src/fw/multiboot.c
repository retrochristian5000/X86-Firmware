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

// SeaVGABIOS's coreboot framebuffer backend is already able to provide INT10
// and VBE services on top of a linear framebuffer.  The VGA option ROM is a
// separate binary, though, so it cannot read entry_elf_ebx directly.  Publish
// GRUB's Multiboot framebuffer as the small coreboot table that backend already
// understands.  0x500 is outside the IVT/BDA and is only populated after all
// Multiboot modules have been copied away from the bootloader handoff data.
#define MULTIBOOT_CB_TABLE_ADDR 0x500
#define CB_SIGNATURE 0x4f49424c // "LBIO"
#define CB_TAG_FRAMEBUFFER 0x0012

struct multiboot_cb_header {
    u32 signature;
    u32 header_bytes;
    u32 header_checksum;
    u32 table_bytes;
    u32 table_checksum;
    u32 table_entries;
};

struct multiboot_cb_framebuffer {
    u32 tag;
    u32 size;
    u64 physical_address;
    u32 x_resolution;
    u32 y_resolution;
    u32 bytes_per_line;
    u8 bits_per_pixel;
    u8 red_mask_pos;
    u8 red_mask_size;
    u8 green_mask_pos;
    u8 green_mask_size;
    u8 blue_mask_pos;
    u8 blue_mask_size;
    u8 reserved_mask_pos;
    u8 reserved_mask_size;
};

struct multiboot_cb_table {
    struct multiboot_cb_header header;
    struct multiboot_cb_framebuffer framebuffer;
};

static u16
multiboot_ipchksum(void *buf, int count)
{
    u16 *p = buf;
    u32 sum = 0;
    while (count > 1) {
        sum += *p++;
        count -= 2;
    }
    if (count)
        sum += *(u8 *)p;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += sum >> 16;
    return ~sum;
}

static void
multiboot_prepare_vga(struct multiboot_info *mbi)
{
    if (!(mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)
        || mbi->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB
        || !mbi->framebuffer_addr || mbi->framebuffer_addr > 0xffffffff
        || !mbi->framebuffer_pitch || !mbi->framebuffer_width
        || !mbi->framebuffer_height)
        return;

    u8 bpp = mbi->framebuffer_bpp;
    if (bpp != 15 && bpp != 16 && bpp != 24 && bpp != 32)
        return;

    u8 rpos = mbi->framebuffer_red_field_position;
    u8 rsize = mbi->framebuffer_red_mask_size;
    u8 gpos = mbi->framebuffer_green_field_position;
    u8 gsize = mbi->framebuffer_green_mask_size;
    u8 bpos = mbi->framebuffer_blue_field_position;
    u8 bsize = mbi->framebuffer_blue_mask_size;
    u8 rgb_size = rsize + gsize + bsize;
    if (!rsize || !gsize || !bsize || rgb_size > bpp
        || rpos + rsize > bpp || gpos + gsize > bpp || bpos + bsize > bpp)
        return;

    u64 rmask = ((1ULL << rsize) - 1) << rpos;
    u64 gmask = ((1ULL << gsize) - 1) << gpos;
    u64 bmask = ((1ULL << bsize) - 1) << bpos;
    if ((rmask & gmask) || (rmask & bmask) || (gmask & bmask))
        return;

    u32 bypp = (bpp + 7) / 8;
    u64 minimum_pitch = (u64)mbi->framebuffer_width * bypp;
    u64 framebuffer_size = (u64)mbi->framebuffer_pitch
                           * mbi->framebuffer_height;
    if (mbi->framebuffer_pitch < minimum_pitch
        || mbi->framebuffer_addr + framebuffer_size > 0x100000000ULL)
        return;

    u8 used_end = rpos + rsize;
    if (used_end < gpos + gsize)
        used_end = gpos + gsize;
    if (used_end < bpos + bsize)
        used_end = bpos + bsize;
    u8 reserved_size = bpp - rgb_size;

    struct multiboot_cb_table *table = (void *)MULTIBOOT_CB_TABLE_ADDR;
    memset(table, 0, sizeof(*table));
    table->header.signature = CB_SIGNATURE;
    table->header.header_bytes = sizeof(table->header);
    table->header.table_bytes = sizeof(table->framebuffer);
    table->header.table_entries = 1;

    table->framebuffer.tag = CB_TAG_FRAMEBUFFER;
    table->framebuffer.size = sizeof(table->framebuffer);
    table->framebuffer.physical_address = mbi->framebuffer_addr;
    table->framebuffer.x_resolution = mbi->framebuffer_width;
    table->framebuffer.y_resolution = mbi->framebuffer_height;
    table->framebuffer.bytes_per_line = mbi->framebuffer_pitch;
    table->framebuffer.bits_per_pixel = bpp;
    table->framebuffer.red_mask_pos = rpos;
    table->framebuffer.red_mask_size = rsize;
    table->framebuffer.green_mask_pos = gpos;
    table->framebuffer.green_mask_size = gsize;
    table->framebuffer.blue_mask_pos = bpos;
    table->framebuffer.blue_mask_size = bsize;
    table->framebuffer.reserved_mask_pos = used_end;
    table->framebuffer.reserved_mask_size = reserved_size;

    table->header.table_checksum = multiboot_ipchksum(
        &table->framebuffer, sizeof(table->framebuffer));
    table->header.header_checksum = multiboot_ipchksum(
        &table->header, sizeof(table->header));

    dprintf(1, "Using Multiboot framebuffer @ %llx %dx%d with %d bpp (%d stride).\n",
            mbi->framebuffer_addr, mbi->framebuffer_width,
            mbi->framebuffer_height, bpp, mbi->framebuffer_pitch);
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
    if (mbi->flags & MULTIBOOT_INFO_MODS) {
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

    multiboot_prepare_vga(mbi);
}
