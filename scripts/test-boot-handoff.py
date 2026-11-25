#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the real disk boot and handoff functions with mocked BIOS I/O."""

from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/boot.c").read_text(encoding="utf-8")
start = source.index("static void\ncall_boot_entry(")
end = source.index("// Boot from a CD-ROM", start)

STUB = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
#define F_IF 0x0200
#define F_CF 0x0001
#define MBR_SIGNATURE 0xaa55
#define dprintf(...) ((void)0)

struct segoff_s { u16 seg, offset; };
#define SEGOFF(s, o) ((struct segoff_s){ (s), (o) })
struct bregs {
    u16 flags, ax, es, ds;
    u8 dl, ah, al, cl;
    struct segoff_s code;
};
struct mbr_s { u8 data[510]; u16 signature; };
static struct mbr_s sector;
static u8 expected_drive;
static int reads, measures, entries;
enum read_mode { GOOD, UNSIGNED, ERROR, NO_HANDLER, NO_TRANSFER };
static enum read_mode mode;

static u16 *mock_signature(u16 seg, size_t size)
{
    assert(seg == 0x07c0 && size == sizeof(u16));
    return &sector.signature;
}
#define GET_FARVAR(seg, var) (*mock_signature((seg), sizeof(var)))
#define SET_FARVAR(seg, var, val) (GET_FARVAR((seg), (var)) = (val))
static void *MAKE_FLATPTR(u16 seg, unsigned offset)
{
    assert(seg == 0x07c0 && offset == 0);
    return &sector;
}
static void call16_int(int nr, struct bregs *br)
{
    assert(nr == 0x13 && reads++ == 0);
    assert(br->flags == (F_IF | F_CF));
    assert(br->dl == expected_drive && br->es == 0x07c0);
    assert(br->ah == 2 && br->al == 1 && br->cl == 1);
    assert(sector.signature == 0);
    if (mode == NO_HANDLER)
        return;
    if (mode == ERROR) {
        sector.signature = MBR_SIGNATURE;
        return;
    }
    br->flags &= ~F_CF;
    if (mode == GOOD)
        sector.signature = MBR_SIGNATURE;
}
static void tpm_add_bcv(u8 drive, void *buf, unsigned count)
{
    assert(reads == 1 && measures++ == 0 && entries == 0);
    assert(drive == expected_drive && buf == &sector && count == 512);
}
static void farcall16(struct bregs *br)
{
    assert(measures == 1 && entries++ == 0);
    assert(br->flags == F_IF && br->ax == MBR_SIGNATURE);
    assert(br->dl == expected_drive && br->ds == 0 && br->es == 0);
    assert(br->code.seg == 0 && br->code.offset == 0x7c00);
}
"""

CHECKS = r"""
static void run_case(enum read_mode read, u8 drive, int checksig, int success)
{
    mode = read;
    expected_drive = drive;
    reads = measures = entries = 0;
    sector.signature = MBR_SIGNATURE; // Stale signature from previous boot.
    boot_disk(drive, checksig);
    assert(reads == 1 && measures == success && entries == success);
}

int main(void)
{
    run_case(GOOD, 0x80, 1, 1);
    run_case(GOOD, 0x00, 1, 1);
    run_case(UNSIGNED, 0x80, 1, 0);
    run_case(ERROR, 0x80, 1, 0);
    run_case(NO_HANDLER, 0x00, 0, 0);
    run_case(NO_TRANSFER, 0x80, 1, 0);
    run_case(UNSIGNED, 0x00, 0, 1);
    return 0;
}
"""

cc = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
if not cc:
    raise SystemExit("native C compiler required")

with tempfile.TemporaryDirectory(prefix="seabios-boot-handoff-") as temp:
    program = Path(temp) / "boot-handoff.c"
    binary = Path(temp) / "boot-handoff"
    program.write_text(STUB + source[start:end] + CHECKS, encoding="utf-8")
    subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    str(program), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

print("SeaBIOS disk boot handoff: 7 cases passed")
