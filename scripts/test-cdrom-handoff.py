#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise El Torito boot catalog validation and image-read completion."""

from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/cdrom.c").read_text(encoding="utf-8")
start = source.index("int\ncdrom_boot(")
end = source.index("// check if media is present", start)

STUB = r"""
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define ASSERT32FLAT() ((void)0)
#define dprintf(...) ((void)0)
#define CDROM_SECTOR_SIZE 2048
#define CMD_READ 2
#define EXTTYPE_CD 2
#define EXTSTART_CD 0xe0
#define CONFIG_CDROM_EMU 1
#define GET_SEG(x) 0
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define GET_BDA(x) (hdcount)
#define SET_BDA(x, v) (hdcount = (v))

struct drive_s { int cntl_id; };
struct disk_op_s {
    struct drive_s *drive_fl;
    int command, count;
    u32 lba;
    void *buf_fl;
};
struct chs_s { u8 sptcyl, cyllow, heads; };
struct mbr_s { struct { struct chs_s last; } partitions[4]; };
static struct {
    u16 load_segment, buffer_segment, sector_count;
    u32 ilba;
    u8 controller_index, device_spec, emulated_drive, media;
    struct chs_s chs;
} CDEmu;
static struct drive_s drive, *emulated_drive_gf, *cdemu_drive_gf = &drive;
static int hdcount;
static u8 image[2 * CDROM_SECTOR_SIZE];
static int reads, measured;
enum mode { GOOD, BAD_SUM, BAD_MEDIA, EMPTY, SHORT_RECORD,
            SHORT_CATALOG, SHORT_IMAGE, IO_ERROR };
static enum mode mode;

static int getDriveId(int type, struct drive_s *d)
{
    assert(type == EXTTYPE_CD && d == &drive);
    return 0;
}
static int scsi_is_ready(struct disk_op_s *op)
{
    assert(op->drive_fl == &drive);
    return 0;
}
static void *flatptr(unsigned seg, uintptr_t offset)
{
    if (seg == 0)
        return (void *)offset;
    assert(seg == 0x07c0 && offset == 0);
    return image;
}
#define MAKE_FLATPTR(seg, off) flatptr((seg), (uintptr_t)(off))
static void tpm_add_cdrom_catalog(void *buf, size_t size)
{
    assert(buf && size == CDROM_SECTOR_SIZE);
    measured++;
}
static void set_equipment_flags(int mask, int value)
{
    (void)mask;
    (void)value;
}
static int process_op(struct disk_op_s *op)
{
    assert(op->command == CMD_READ);
    u8 *buf = op->buf_fl;
    switch (reads++) {
    case 0:
        assert(op->lba == 0x11 && op->count == 1);
        if (mode == SHORT_RECORD) {
            op->count = 0;
            return 0;
        }
        memset(buf, 0, CDROM_SECTOR_SIZE);
        memcpy(buf + 1, "CD001\001EL TORITO SPECIFICATION",
               sizeof("CD001\001EL TORITO SPECIFICATION"));
        buf[0x47] = 0x20;
        return 0;
    case 1:
        assert(op->lba == 0x20 && op->count == 1);
        if (mode == SHORT_CATALOG) {
            op->count = 0;
            return 0;
        }
        memset(buf, 0, CDROM_SECTOR_SIZE);
        buf[0] = 1;
        buf[0x1c] = 0xaa;
        buf[0x1d] = 0x55;
        buf[0x1e] = 0x55;
        buf[0x1f] = 0xaa;
        if (mode == BAD_SUM)
            buf[0x1c]++;
        buf[0x20] = 0x88;
        buf[0x21] = mode == BAD_MEDIA ? 5 : 0;
        buf[0x26] = mode == EMPTY ? 0 : 8;
        buf[0x28] = 0x30;
        return 0;
    case 2:
        assert(op->lba == 0x30 && op->count == 2 && buf == image);
        if (mode == IO_ERROR)
            return 1;
        if (mode == SHORT_IMAGE)
            op->count = 1;
        return 0;
    default:
        assert(0);
        return 1;
    }
}
"""

CHECKS = r"""
static void run_case(enum mode m, int status, int nreads, int nmeasures)
{
    mode = m;
    reads = measured = 0;
    memset(&CDEmu, 0, sizeof(CDEmu));
    emulated_drive_gf = NULL;
    assert(cdrom_boot(&drive) == status);
    assert(reads == nreads && measured == nmeasures);
    if (status == 0) {
        assert(CDEmu.load_segment == 0x07c0);
        assert(CDEmu.emulated_drive == EXTSTART_CD);
        assert(CDEmu.sector_count == 8 && CDEmu.media == 0);
    } else if (nmeasures == 0) {
        assert(emulated_drive_gf == NULL);
    }
}

int main(void)
{
    run_case(GOOD, 0, 3, 1);
    run_case(BAD_SUM, 10, 2, 0);
    run_case(BAD_MEDIA, 11, 2, 0);
    run_case(EMPTY, 11, 2, 0);
    run_case(SHORT_RECORD, 3, 1, 0);
    run_case(SHORT_CATALOG, 7, 2, 0);
    run_case(SHORT_IMAGE, 12, 3, 1);
    run_case(IO_ERROR, 12, 3, 1);
    return 0;
}
"""

cc = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
if not cc:
    raise SystemExit("native C compiler required")

with tempfile.TemporaryDirectory(prefix="seabios-cdrom-handoff-") as temp:
    program = Path(temp) / "cdrom-handoff.c"
    binary = Path(temp) / "cdrom-handoff"
    program.write_text(STUB + source[start:end] + CHECKS, encoding="utf-8")
    subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    str(program), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

# Verify that boot registration no longer accesses removable media, and
# that the optional label lookup is only reached after opening the menu.
boot = (ROOT / "src/boot.c").read_text(encoding="utf-8")
registration = boot[boot.index("void\nboot_add_cd("):
                    boot.index("// Add a CBFS payload entry")]
assert "cdrom_media_info(" not in registration
assert "bootentry_add(IPL_TYPE_CDROM" in registration
menu = boot[boot.index("void\ninteractive_bootmenu("):
            boot.index("// BEV (Boot Execution Vector) list")]
label = menu.index("cdrom_media_info(pos->drive)")
assert menu.index("if (scan_code != menukey)") < label
assert menu.index("wait_threads();") < label
assert menu.index("maxmenu >= ARRAY_SIZE(menuchars)") < label
assert menu.index("pos->type == IPL_TYPE_CDROM") < label
assert menu.index("PF_QEMU", menu.index("pos->type == IPL_TYPE_CDROM")) < label
assert "free(labelled);" in menu[label:]
assert "buffer[40 + 32] = 0;" in source[end:]
assert "if (volume)\n        nullTrailingSpace(volume);" in source[end:]

print("SeaBIOS El Torito handoff: 8 cases passed; lazy menu labels checked")
