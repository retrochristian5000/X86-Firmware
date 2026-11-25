#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check legacy INT 10h palette dispatch without touching VGA hardware."""

from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "vgasrc/vgabios.c").read_text(encoding="utf-8")
start = source.index("static void\nhandle_100b00(")
end = source.index("\nstatic void", source.index("\nhandle_100b(struct", start))

STUB = r"""
#include <assert.h>
#include <stdint.h>

typedef uint8_t u8;
struct bregs { u8 bh, bl; };
static u8 video_mode;
static int ports = 1;
#define GET_BDA(field) (field)
#define CONFIG_VGA_STDVGA_PORTS ports
static unsigned reads, border, background, palette, unsupported;
static u8 value;
static const u8 mapping[16] = {
    0, 1, 2, 3, 4, 5, 0x14, 7,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
};
static u8 stdvga_attr_read(u8 index)
{
    assert(index < 16);
    reads++;
    return mapping[index];
}
static void stdvga_set_overscan_border_color(u8 color)
{ border++; value = color; }
static void stdvga_set_cga_background_color(u8 color)
{ background++; value = color; }
static void stdvga_set_cga_palette(u8 color)
{ palette++; value = color; }
static void debug_stub(struct bregs *regs)
{ (void)regs; unsupported++; }
"""

CHECKS = r"""
static void check(u8 mode, u8 function, u8 color, int expected, u8 result)
{
    struct bregs regs = { function, color };
    video_mode = mode;
    reads = border = background = palette = unsupported = 0;
    value = 0;
    handle_100b(&regs);
    assert(regs.bh == function && regs.bl == color);
    assert(reads == (expected == 1));
    assert(border == (expected == 1));
    assert(background == (expected == 2));
    assert(palette == (expected == 3));
    assert(unsupported == (expected == 4));
    if (expected != 4)
        assert(value == result);
}

int main(void)
{
    (void)stdvga_attr_read;
    (void)stdvga_set_overscan_border_color;
    for (u8 mode = 0; mode <= 3; mode++) {
        for (u8 color = 0; color < 32; color++)
            check(mode, 0, color, 1, mapping[color & 15]);
        check(mode, 1, 1, 4, 0);
    }
    for (u8 mode = 4; mode <= 5; mode++) {
        check(mode, 0, 0x16, 2, 0x16);
        check(mode, 1, 0, 3, 0);
        check(mode, 1, 1, 3, 1);
    }
    check(6, 0, 0x16, 2, 0x16);
    check(6, 1, 1, 4, 0);
    check(0x0d, 1, 1, 4, 0);
    check(0x12, 1, 1, 4, 0);
    check(0x13, 1, 1, 4, 0);
    check(4, 2, 1, 4, 0);
    ports = 0;
    check(3, 0, 6, 4, 0);
    check(4, 1, 1, 4, 0);
    return 0;
}
"""

cc = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
if not cc:
    raise SystemExit("native C compiler required")
with tempfile.TemporaryDirectory(prefix="seabios-cga-palette-") as temp:
    program = Path(temp) / "palette.c"
    binary = Path(temp) / "palette"
    program.write_text(STUB + source[start:end] + CHECKS, encoding="utf-8")
    subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    str(program), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("SeaBIOS CGA palette dispatch checks passed")
