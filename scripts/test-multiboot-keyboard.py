#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
multiboot = (ROOT / 'src/fw/multiboot.c').read_text(encoding='utf-8')
post = (ROOT / 'src/post.c').read_text(encoding='utf-8')
util = (ROOT / 'src/util.h').read_text(encoding='utf-8')
kbd = (ROOT / 'src/kbd.c').read_text(encoding='utf-8')
ps2 = (ROOT / 'src/hw/ps2port.c').read_text(encoding='utf-8')

# The Multiboot profile enters from EFI GRUB, not from a reset/coreboot path.
# Restore the local APIC virtual-wire input before SeaBIOS enables IRQ1 so the
# legacy PIC keyboard interrupt can reach entry_09.
required_bridge = (
    'multiboot_setup_legacy_irqs(void)',
    'MSR_IA32_APIC_BASE',
    'MSR_IA32_APICBASE_EXTD',
    'MSR_X2APIC_SVR',
    'MSR_X2APIC_LVT_LINT0',
    'MSR_X2APIC_LVT_LINT1',
    'APIC_LINT0_EXTINT',
    'APIC_LINT1_NMI',
)
for token in required_bridge:
    assert token in multiboot, f'missing Multiboot legacy IRQ bridge token: {token}'

assert 'void multiboot_setup_legacy_irqs(void);' in util
assert 'multiboot_setup_legacy_irqs();' in post
assert post.index('pic_setup();') < post.index('multiboot_setup_legacy_irqs();') < post.index('thread_setup();'), (
    'Multiboot legacy IRQ routing must be restored after PIC reset and before threads/keyboard setup'
)

# Keep the actual BIOS keyboard path present: IRQ1 feeds process_key(), and
# INT16 status/read calls consume the BDA ring buffer populated by that path.
for token in (
    'SET_IVT(0x16, FUNC16(entry_16));',
    'case 0x00: handle_1600(regs); break;',
    'case 0x01: handle_1601(regs); break;',
):
    assert token in post + kbd, f'missing INT16 keyboard service token: {token}'
assert 'enable_hwirq(1, FUNC16(entry_09));' in ps2
assert 'process_key(v);' in ps2

print('Multiboot GRUB keyboard IRQ/INT16 bridge: verified')
