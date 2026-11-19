#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Ensure the WHP memory report is opt-in and never changes guest E820."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
post = (ROOT / 'src/post.c').read_text(encoding='utf-8')
memory = (ROOT / 'src/malloc.c').read_text(encoding='utf-8')
header = (ROOT / 'src/malloc.h').read_text(encoding='utf-8')
docs = (ROOT / 'docs/Debugging.md').read_text(encoding='utf-8')

assert 'romfile_find("opt/org.seabios/whp-memory-report")' in post
assert 'CONFIG_DEBUG_LEVEL && CONFIG_QEMU' in post
assert 'void malloc_report_memory(void);' in header
assert 'void\nmalloc_report_memory(void)' in memory
assert 'WHP SeaBIOS memory: conventional=%u KiB EBDA=%u KiB' in memory

# Preserve POST and E820 order; only inspect once memory is finalized.
start = post.index('static void\nmaininit(void)')
main = post[start:post.index('reloc_preinit(void *f, void *arg)', start)]
assert main.index('prepareboot();') < main.index('malloc_report_memory();')
prep = post.index('void\nprepareboot(void)')
prep_body = post[prep:post.index('void VISIBLE32FLAT\nstartBoot(void)', prep)]
assert prep_body.index('malloc_prepboot();') < prep_body.index('e820_prepboot();')

# Diagnostics are read-only: never allocate, move EBDA, alter the memory
# map, or change an allocation-zone boundary to obtain a measurement.
report = memory.split('void\nmalloc_report_memory(void)', 1)[1].split(
    '\nvoid\nmalloc_prepboot(void)', 1
)[0]
for forbidden in ('e820_add(', 'e820_remove(', 'alloc_new(', 'alloc_add(',
                  'malloc_palloc(', 'relocate_ebda(', 'zonelow_expand('):
    assert forbidden not in report, forbidden
for required in ('GET_BDA(mem_size_kb)', 'GET_EBDA(ebda_seg, size)',
                 'zone_dynamic_bytes(&ZoneLow)', 'zone_dynamic_bytes(&ZoneFSeg)',
                 'zone_dynamic_bytes(&ZoneHigh)',
                 'ALIGN_DOWN(free_tail, PAGE_SIZE)'):
    assert required in report, required

# The output must explain its units and not equate allocations with all
# reserved guest memory or host process memory.
assert 'whole pages returned' in docs
assert 'not the total QEMU process RSS' in ' '.join(docs.split())
assert 'E820 reservations also include non-BIOS hardware regions' in memory

print('SeaBIOS opt-in guest-memory report: verified')
