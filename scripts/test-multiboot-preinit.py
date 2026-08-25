#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
multiboot = (ROOT / 'src/fw/multiboot.c').read_text(encoding='utf-8')
post = (ROOT / 'src/post.c').read_text(encoding='utf-8')
util = (ROOT / 'src/util.h').read_text(encoding='utf-8')

required_multiboot = (
    '#include "e820map.h"',
    'multiboot_preinit(void)',
    'MULTIBOOT_INFO_MEM_MAP',
    'MULTIBOOT_MEMORY_AVAILABLE',
    'E820_RAM',
    'MULTIBOOT_MEMORY_RESERVED',
    'E820_RESERVED',
    'MULTIBOOT_MEMORY_ACPI_RECLAIMABLE',
    'E820_ACPI',
    'MULTIBOOT_MEMORY_NVS',
    'E820_NVS',
    'MULTIBOOT_MEMORY_BADRAM',
    'E820_UNUSABLE',
    'mbi->mmap_addr',
    'mbi->mmap_length',
    'mbi->mods_addr',
    'mod_start',
    'mod_end',
)
for token in required_multiboot:
    assert token in multiboot, f'missing Multiboot pre-init token: {token}'

coreboot_preinit = post.index('coreboot_preinit();')
multiboot_preinit = post.index('multiboot_preinit();')
malloc_preinit = post.index('malloc_preinit();')
assert coreboot_preinit < multiboot_preinit < malloc_preinit, (
    'GRUB memory map must replace the coreboot fallback before allocator setup'
)

assert 'void multiboot_preinit(void);' in util

print('early Multiboot memory handoff: verified')
