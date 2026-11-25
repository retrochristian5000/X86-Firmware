#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise SeaBIOS CD emulation buffers and floppy DMA range validation."""

from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
cdrom = (ROOT / "src/cdrom.c").read_text(encoding="utf-8")
start = cdrom.index("static int\ncdemu_read(")
end = cdrom.index("\nint\ncdemu_process_op(", start)
dma = (ROOT / "src/hw/dma.c").read_text(encoding="utf-8")
dma_start = dma.index("#define PORT_DMA_ADDR_2")
dma_end = dma.index("\nvoid\ndma_setup(", dma_start)

STUB = r"""
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#define DISK_SECTOR_SIZE 512
#define CDROM_SECTOR_SIZE 2048
#define DISK_RET_SUCCESS 0
#define DISK_RET_ECONTROLLER 0x20
#define DISK_RET_ENOTREADY 0xaa
#define CMD_READ 2
#define GET_LOW(x) (x)
#define GET_GLOBAL(x) (x)
#define memcpy_fl memcpy

struct drive_s { int unused; };
struct disk_op_s {
    void *buf_fl;
    struct drive_s *drive_fl;
    u8 command;
    u16 count;
    u64 lba;
};
static struct { u32 ilba; } CDEmu = { 20 };
static struct drive_s drive;
static struct drive_s *emulated_drive_gf = &drive;
static u8 bounce[CDROM_SECTOR_SIZE];
static u8 *bounce_buf_fl = bounce;
static u8 destination[128 * DISK_SECTOR_SIZE + 2];

static int reads, fault_at, fault_count, fault_status;
static int process_op(struct disk_op_s *op)
{
    assert(op->drive_fl == &drive && op->command == CMD_READ);
    int requested = op->count;
    if (++reads == fault_at) {
        op->count = fault_count;
        if (op->count > requested)
            return fault_status;
    }
    u8 *buf = op->buf_fl;
    for (int i = 0; i < op->count * CDROM_SECTOR_SIZE; i++)
        buf[i] = (u8)((op->lba - CDEmu.ilba) * 4 + i / DISK_SECTOR_SIZE);
    return reads == fault_at ? fault_status : 0;
}

static unsigned writes;
static u8 count_bytes[2];
static unsigned count_writes;
static void outb(u8 value, u16 port)
{
    writes++;
    if (port == 5) {
        assert(count_writes < 2);
        count_bytes[count_writes++] = value;
    }
}
"""

CHECKS = r"""
static void read_case(u64 lba, u16 count, int fail_at, int returned,
                      int status, int expected, int transferred, int calls,
                      int have_bounce)
{
    memset(destination, 0xa5, sizeof(destination));
    memset(bounce, 0x5a, sizeof(bounce));
    reads = 0;
    fault_at = fail_at;
    fault_count = returned;
    fault_status = status;
    bounce_buf_fl = have_bounce ? bounce : NULL;
    struct disk_op_s op = {
        .buf_fl = destination + 1, .drive_fl = &drive,
        .command = CMD_READ, .count = count, .lba = lba,
    };
    assert(cdemu_read(&op) == expected);
    assert(op.count == transferred && reads == calls);
    assert(op.buf_fl == destination + 1);
    for (int i = 0; i < transferred * DISK_SECTOR_SIZE; i++)
        assert(destination[i + 1] == (u8)(lba + i / DISK_SECTOR_SIZE));
    assert(destination[0] == 0xa5);
    for (size_t i = 1 + transferred * DISK_SECTOR_SIZE;
         i < sizeof(destination); i++)
        assert(destination[i] == 0xa5);
}

static void dma_case(u32 addr, int count, int expected)
{
    writes = count_writes = 0;
    assert(dma_floppy(addr, count, 0) == expected);
    if (expected == 0) {
        assert(writes && count_writes == 2);
        assert((count_bytes[0] | (count_bytes[1] << 8)) == count - 1);
    } else {
        assert(writes == 0);
    }
}

int main(void)
{
    read_case(0, 0, 0, 0, 0, 0, 0, 0, 0);
    read_case(1, 1, 0, 0, 0, 0, 1, 1, 1);
    read_case(1, 8, 0, 0, 0, 0, 8, 3, 1);
    read_case(0, 128, 0, 0, 0, 0, 128, 1, 0);
    read_case(1, 8, 1, 0, 0, DISK_RET_ECONTROLLER, 0, 1, 1);
    read_case(0, 9, 1, 1, 0, DISK_RET_ECONTROLLER, 4, 1, 1);
    read_case(0, 9, 1, 1, 0x80, 0x80, 4, 1, 1);
    read_case(0, 9, 1, 3, 0, DISK_RET_ECONTROLLER, 0, 1, 1);
    read_case(1, 8, 3, 0, 0, DISK_RET_ECONTROLLER, 7, 3, 1);
    read_case(0, 1, 0, 0, 0, DISK_RET_ENOTREADY, 0, 0, 0);
    read_case(1, 4, 0, 0, 0, DISK_RET_ENOTREADY, 0, 0, 0);

    dma_case(0x10000, 0x10000, 0);
    dma_case(0xffff, 1, 0);
    dma_case(0xffffff, 1, 0);
    dma_case(0x10000, 0, -1);
    dma_case(0x10000, -1, -1);
    dma_case(0x10000, 0x10001, -1);
    dma_case(0xffff, 2, -1);
    dma_case(0x1000000, 1, -1);
    dma_case(0xffffff, 2, -1);
    dma_case(0xffffffff, 2, -1);
    return 0;
}
"""

cc = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
if not cc:
    raise SystemExit("native C compiler required")

with tempfile.TemporaryDirectory(prefix="seabios-disk-buffers-") as temp:
    program = Path(temp) / "disk-buffers.c"
    binary = Path(temp) / "disk-buffers"
    program.write_text(
        STUB + cdrom[start:end] + dma[dma_start:dma_end] + CHECKS,
        encoding="utf-8",
    )
    subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    str(program), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

print("SeaBIOS disk buffers: 21 cases passed")
