#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
multiboot = (ROOT / 'src/fw/multiboot.c').read_text(encoding='utf-8')

required_multiboot = (
    'multiboot_prepare_vga(struct multiboot_info *mbi)',
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
    'multiboot_prepare_vga(mbi);',
)
for token in required_multiboot:
    assert token in multiboot, f'missing Multiboot framebuffer token: {token}'

module_copy = multiboot.index('romfile_add(&cfile->file);')
prepare = multiboot.index('multiboot_prepare_vga(mbi);')
assert module_copy < prepare, (
    'GRUB modules must be copied before the low-memory framebuffer table is published'
)

assert 'mbi->framebuffer_addr > 0xffffffff' in multiboot
assert 'mbi->framebuffer_addr + framebuffer_size > 0x100000000ULL' in multiboot

print('Multiboot framebuffer handoff: verified')
