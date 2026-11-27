// Code for emulating a drive via high-memory accesses.
//
// Copyright (C) 2009  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "biosvar.h" // GET_GLOBALFLAT
#include "block.h" // struct drive_s
#include "e820map.h" // e820_add
#include "malloc.h" // memalign_tmphigh
#include "memmap.h" // PAGE_SIZE
#include "output.h" // dprintf
#include "romfile.h" // romfile_findprefix
#include "std/disk.h" // DISK_RET_SUCCESS
#include "string.h" // memcpy
#include "util.h" // process_ramdisk_op

void
ramdisk_setup(void)
{
    if (!CONFIG_FLASH_FLOPPY)
        return;

    // Find image.
    struct romfile_s *file = romfile_findprefix("floppyimg/", NULL);
    if (!file)
        return;
    const char *filename = file->name;
    u32 size = file->size;
    dprintf(3, "Found floppy file %s of size %d\n", filename, size);
    int ftype = find_floppy_type(size);
    if (ftype < 0) {
        dprintf(3, "No floppy type found for ramdisk size\n");
        return;
    }

    // Allocate ram for image.
    void *pos = memalign_tmphigh(PAGE_SIZE, size);
    if (!pos) {
        warn_noalloc();
        return;
    }
    e820_add((u32)pos, size, E820_RESERVED);

    // Copy image into ram.
    int ret = file->copy(file, pos, size);
    if (ret < 0)
        return;

    // Setup driver.
    struct drive_s *drive = init_floppy((u32)pos, ftype);
    if (!drive)
        return;
    drive->type = DTYPE_RAMDISK;
    drive->sectors = size / DISK_SECTOR_SIZE;
    dprintf(1, "Mapping floppy %s to addr %p\n", filename, pos);
    char *desc = znprintf(MAXDESCSIZE, "Ramdisk [%s]", &filename[10]);
    boot_add_floppy(drive, desc, bootprio_find_named_rom(filename, 0));
}

// The RAM disk is already in guest memory.  Use the normal flat-mode
// disk dispatcher instead of nesting an INT 15h block-move service.
static int
ramdisk_copy(struct disk_op_s *op, int iswrite)
{
    ASSERT32FLAT();
    if (!op->count)
        return DISK_RET_SUCCESS;
    u64 sectors = op->drive_fl->sectors;
    if (op->lba >= sectors || op->count > sectors - op->lba)
        return DISK_RET_EADDRNOTFOUND;

    u32 bytes = (u32)op->count * DISK_SECTOR_SIZE;
    u32 base = op->drive_fl->cntl_id;
    u32 buf = (u32)op->buf_fl;
    u64 addr = (u64)base + op->lba * DISK_SECTOR_SIZE;
    // Keep the disk request limit and reject wraparound in either range.
    if (bytes > 64 * 1024 || !buf ||
        addr + bytes > (1ULL << 32) || (u64)buf + bytes > (1ULL << 32))
        return DISK_RET_EBOUNDARY;

    // An overlapping buffer is not a valid independent disk-transfer area.
    if ((u64)buf < addr + bytes && addr < (u64)buf + bytes)
        return DISK_RET_EBOUNDARY;

    void *disk = (void *)(u32)addr;
    if (iswrite)
        memcpy(disk, op->buf_fl, bytes);
    else
        memcpy(op->buf_fl, disk, bytes);
    return DISK_RET_SUCCESS;
}

int
ramdisk_process_op(struct disk_op_s *op)
{
    if (!CONFIG_FLASH_FLOPPY)
        return 0;

    switch (op->command) {
    case CMD_READ:
        return ramdisk_copy(op, 0);
    case CMD_WRITE:
        return ramdisk_copy(op, 1);
    default:
        return default_process_op(op);
    }
}
