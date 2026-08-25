#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
multiboot = (ROOT / 'src/fw/multiboot.c').read_text(encoding='utf-8')
post = (ROOT / 'src/post.c').read_text(encoding='utf-8')
util = (ROOT / 'src/util.h').read_text(encoding='utf-8')
layoutrom = (ROOT / 'scripts/layoutrom.py').read_text(encoding='utf-8')

required_multiboot = (
    'multiboot_prepare_vga(void)',
    'MULTIBOOT_INFO_FRAMEBUFFER_INFO',
    'MULTIBOOT_FRAMEBUFFER_TYPE_RGB',
    'framebuffer_addr',
    'framebuffer_pitch',
    'framebuffer_width',
    'framebuffer_height',
    'framebuffer_bpp',
    'CB_TAG_FRAMEBUFFER',
    'MULTIBOOT_CB_TABLE_ADDR',
    'table_checksum',
    'header_checksum',
)
for token in required_multiboot:
    assert token in multiboot, f'missing Multiboot framebuffer token: {token}'

prepare = post.index('multiboot_prepare_vga();')
vgarom = post.index('vgarom_setup();')
assert prepare < vgarom, 'GRUB framebuffer handoff must exist before VGA ROM init'
assert 'void multiboot_prepare_vga(void);' in util

assert 'MULTIBOOT_VIDEO_MODE' in layoutrom, (
    'Multiboot payload must request video information from GRUB'
)

print('Multiboot framebuffer handoff: verified')
