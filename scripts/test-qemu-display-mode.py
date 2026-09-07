#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
bootsplash = (ROOT / "src/bootsplash.c").read_text(encoding="utf-8")

required = (
    'romfile_loadint("etc/qemu-display-width", 0)',
    'romfile_loadint("etc/qemu-display-height", 0)',
    'romfile_loadint("etc/qemu-display-depth", 0)',
    'find_videomode(vesa_info, mode_info, width, height, depth)',
    'VBE_MODE_LINEAR_FRAME_BUFFER',
    'QEMU requested display mode',
)

for token in required:
    assert token in bootsplash, f"missing QEMU display-mode contract: {token}"

console = bootsplash.index("enable_vga_console(void)")
width = bootsplash.index('romfile_loadint("etc/qemu-display-width", 0)')
assert width < console, "QEMU display-mode helper must be available to the VGA console"

fallback = bootsplash.index("br.ax = 0x0003;", console)
print_banner = bootsplash.index('printf("SeaBIOS (version %s)\\n", VERSION);', console)
assert fallback < print_banner, "SeaBIOS must retain VGA text-mode fallback before printing"

print("QEMU -g SeaBIOS display-mode handoff: verified")
