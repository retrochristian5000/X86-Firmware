#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check AHCI descriptor preparation and legacy ATAPI non-data handling."""

from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/hw/ahci.c").read_text(encoding="utf-8")
start = source.index("static int\nahci_prepare_transfer(")
end = source.index("// submit ahci command", start)
# Use the actual structure layouts and command flag definitions.
header = (ROOT / "src/hw/ahci.h").read_text(encoding="utf-8")
header = header.replace('#include "block.h" // struct drive_s', "")
header = header.replace('#include "types.h" // u32', "")
program = r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
struct drive_s { int dummy; };
struct disk_op_s;
""" + header + source[start:end].replace("(u32)buffer", "(u32)(uintptr_t)buffer") + r"""
int main(void)
{
    /* The flexible PRDT array has one entry, just like the firmware. */
    union {
        struct ahci_cmd_s cmd;
        uint32_t words[(sizeof(struct ahci_cmd_s) + 16) / 4];
    } storage;
    struct ahci_cmd_s *cmd = &storage.cmd;
    struct ahci_list_s list;
    void *buffer = (void *)(uintptr_t)0x10000;
    memset(&storage, 0xff, sizeof(storage));
    memset(&list, 0xff, sizeof(list));

    assert(!ahci_prepare_transfer(cmd, &list, 0, 1, 0, 0));
    assert(list.flags == (AHCI_CMD_ATAPI | 5));
    assert(!list.bytes && !cmd->prdt[0].base && !cmd->prdt[0].flags);
    assert(!cmd->prdt[0].baseu && !cmd->prdt[0].res);
    assert(!ahci_prepare_transfer(cmd, &list, 0, 1, buffer, 2048));
    assert(list.flags == ((1u << 16) | AHCI_CMD_ATAPI | 5));
    assert(cmd->prdt[0].base == 0x10000 && cmd->prdt[0].flags == 2047);
    assert(!ahci_prepare_transfer(cmd, &list, 1, 0, buffer, 65536));
    assert(list.flags == ((1u << 16) | AHCI_CMD_WRITE | 5));
    assert(cmd->prdt[0].flags == 65535);
    assert(!ahci_prepare_transfer(cmd, &list, 0, 0, buffer, 4u << 20));
    assert(cmd->prdt[0].flags == 0x3fffff);
    assert(ahci_prepare_transfer(cmd, &list, 0, 0, buffer, (4u << 20) + 2));
    assert(ahci_prepare_transfer(cmd, &list, 0, 0, buffer, 3));
    assert(ahci_prepare_transfer(cmd, &list, 0, 0, 0, 2048));
    assert(!ahci_prepare_transfer(cmd, &list, 0, 0, 0, 0));
    assert(list.flags == 5 && !cmd->prdt[0].flags);
    return 0;
}
"""
cc = shutil.which("cc") or shutil.which("clang")
if not cc:
    raise SystemExit("native C compiler required")
with tempfile.TemporaryDirectory(prefix="seabios-atapi-") as temp:
    test = Path(temp) / "test.c"
    binary = Path(temp) / "test"
    test.write_text(program, encoding="utf-8")
    subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

ata = (ROOT / "src/hw/ata.c").read_text(encoding="utf-8")
pio = ata[ata.index("static int\nata_pio_transfer("):]
assert pio.index("if (!count)") < pio.index("for (;;) {")
packet = ata[ata.index("int\nata_atapi_process_op("):]
assert "ATAPI_REASON_MASK) != ATA_CB_SC_P_CD" in packet
assert packet.index("atapi_read_phases(op,") < packet.index("fail:")
assert "outb(ATA_CB_DC_HD15, iobase2+ATA_CB_DC);" in packet
print("ATAPI descriptor and non-data command checks passed")


# Execute the actual legacy ATAPI phase loop with scripted taskfile values.
start = ata.index("#define ATAPI_PIO_LIMIT")
end = ata.index("// Low-level atapi command transmit function.", start)
header = (ROOT / "src/hw/ata.h").read_text(encoding="utf-8")
regs = "\n".join(line for line in header.splitlines()
                 if line.startswith("#define ATA_CB_"))
phase_test = r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define CDROM_SECTOR_SIZE 2048
#define CONFIG_ATA_PIO32 1
#define dprintf(...) ((void)0)
#define SET_FLATPTR(p, v) ((p) = (v))
""" + regs + r"""
struct disk_op_s { u16 count; void *buf_fl; };
struct phase { int status, reason; u16 bytes; };
static struct phase phases[5];
static unsigned next, active, calls, moved;
static u8 buffer[65537];
static int pause_await_not_bsy(u16 base, u16 alt)
{
    (void)base;
    (void)alt;
    assert(next < 5);
    active = next++;
    return phases[active].status;
}
static u8 inb(u16 reg)
{
    switch (reg) {
    case ATA_CB_SC: return phases[active].reason;
    case ATA_CB_CL: return phases[active].bytes;
    case ATA_CB_CH: return phases[active].bytes >> 8;
    case ATA_CB_ERR: return 0x20;
    default: assert(0); return 0;
    }
}
static void transfer(void *p, unsigned bytes)
{
    assert(p == buffer + moved);
    assert(moved + bytes < sizeof(buffer));
    memset(p, 0x5a, bytes);
    moved += bytes;
    calls++;
}
static void insl_fl(u16 port, void *p, u16 count)
{
    (void)port;
    transfer(p, count * 4u);
}
static void insw_fl(u16 port, void *p, u16 count)
{
    (void)port;
    transfer(p, count * 2u);
}
static u16 inw(u16 port)
{
    (void)port;
    moved++;
    calls++;
    return 0xee5a;
}
""" + ata[start:end] + r"""
static const struct phase done = { ATA_CB_STAT_RDY, 3, 0 };
static struct phase data(u16 bytes)
{
    return (struct phase){ ATA_CB_STAT_DRQ, 2, bytes };
}
static void check(int blocksize, u16 count, u16 limit, int result,
                  unsigned expected_count, unsigned expected_moved,
                  unsigned expected_phases)
{
    struct disk_op_s op = { count, buffer };
    next = active = calls = moved = 0;
    memset(buffer, 0, sizeof(buffer));
    assert(atapi_read_phases(&op, blocksize, limit, 0, 0) == result);
    assert(op.count == expected_count);
    assert(moved == expected_moved && next == expected_phases);
    for (unsigned i = 0; i < moved; i++)
        assert(buffer[i] == 0x5a);
    assert(buffer[moved] == 0);
}
int main(void)
{
    phases[0] = data(ATAPI_PIO_LIMIT);
    phases[1] = data(2048);
    phases[2] = done;
    check(2048, 32, ATAPI_PIO_LIMIT, 0, 32, 65536, 3);
    assert(calls == 2);

    phases[0] = data(512);
    phases[1] = data(1536);
    phases[2] = done;
    check(2048, 1, 2048, 0, 1, 2048, 3);

    phases[0] = data(5);
    phases[1] = done;
    check(5, 1, 5, 0, 1, 5, 2);

    phases[0] = done;
    check(0, 0, 0, 0, 0, 0, 1);
    check(2048, 1, 2048, -1, 0, 0, 1);

    phases[0] = data(2048);
    phases[1] = done;
    check(2048, 2, 4096, -1, 1, 2048, 2);
    phases[0] = data(0);
    check(2048, 1, 2048, -1, 0, 0, 1);
    phases[0] = data(4096);
    check(2048, 1, 2048, -1, 0, 0, 1);
    phases[0] = data(2048);
    phases[0].reason = 0;
    check(2048, 1, 2048, -1, 0, 0, 1);
    phases[0] = (struct phase){ ATA_CB_STAT_RDY, 1, 0 };
    check(0, 0, 0, -1, 0, 0, 1);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="seabios-atapi-phases-") as temp:
    test = Path(temp) / "test.c"
    binary = Path(temp) / "test"
    test.write_text(phase_test, encoding="utf-8")
    subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

print("ATA/ATAPI-4 PIO phase checks passed")


# READ(10) uses a padded 16-byte struct, but must fit a 12-byte packet.
# READ(16) must be rejected before writing any bytes into that packet.
blockcmd = (ROOT / "src/hw/blockcmd.c").read_text(encoding="utf-8")
start = blockcmd.index("int\nscsi_fill_cmd(")
end = blockcmd.index("// Determine if the command", start)
header = (ROOT / "src/hw/blockcmd.h").read_text(encoding="utf-8")
header = header.replace('#include "types.h" // u8', "")
cdb_test = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#define PACKED __attribute__((packed))
#define MODESEGMENT 0
#define CMD_READ 2
#define CMD_WRITE 3
#define CMD_SCSI 0x20
#define GET_FLATPTR(p) (p)
#define cpu_to_be16(x) __builtin_bswap16(x)
#define cpu_to_be32(x) __builtin_bswap32(x)
#define cpu_to_be64(x) __builtin_bswap64(x)
""" + header + r"""
struct drive_s { u16 blksize; };
struct disk_op_s {
    int command;
    u16 count;
    u64 lba;
    struct drive_s *drive_fl;
    void *cdbcmd;
    u16 blocksize;
};
""" + blockcmd[start:end] + r"""
int main(void)
{
    struct drive_s drive = { 2048 };
    struct disk_op_s op = { .command = CMD_READ, .count = 2,
                            .lba = 0x123456, .drive_fl = &drive };
    u8 packet[16];
    memset(packet, 0xa5, sizeof(packet));
    assert(scsi_fill_cmd(&op, packet, 12) == 2048);
    assert(packet[0] == CDB_CMD_READ_10 && packet[4] == 0x34);
    assert(packet[5] == 0x56 && packet[8] == 2);
    for (unsigned i = 12; i < 16; i++)
        assert(packet[i] == 0xa5);
    op.lba = 0x100000000ULL;
    memset(packet, 0xa5, sizeof(packet));
    assert(scsi_fill_cmd(&op, packet, 12) == -1);
    for (unsigned i = 0; i < sizeof(packet); i++)
        assert(packet[i] == 0xa5);
    assert(scsi_fill_cmd(&op, packet, 16) == 2048);
    assert(packet[0] == CDB_CMD_READ_16);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="seabios-atapi-cdb-") as temp:
    test = Path(temp) / "test.c"
    binary = Path(temp) / "test"
    test.write_text(cdb_test, encoding="utf-8")
    subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("ATAPI CDB length checks passed")
