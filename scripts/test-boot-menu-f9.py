#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Source-level guard for F9 keyboard and SeaBIOS boot-menu bindings."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
kbd = (ROOT / "src/kbd.c").read_text(encoding="utf-8")
boot = (ROOT / "src/boot.c").read_text(encoding="utf-8")
docs = (ROOT / "docs/Runtime_config.md").read_text(encoding="utf-8")

# INT 16h delivers a make-code of 43h for an unmodified F9; modifiers
# retain their distinct BIOS scan codes.
assert "{ 0x4300, 0x5c00, 0x6600, 0x7000 }, /* F9 */" in kbd
assert 'case 0x00: handle_1600(regs); break;' in kbd
assert 'case 0x01: handle_1601(regs); break;' in kbd

# No new function-key BIOS Setup stub: only the existing configurable
# boot-device menu may use F9.
assert 'romfile_loadint("etc/boot-menu-key", 1)' in boot
assert 'if (scan_code != menukey)' in boot
assert 'menukey >= 0x3b && menukey <= 0x44' in boot
assert 'menukey - 0x3a' in boot
assert 'menukey == 1' in boot
assert 'if (bootmsg)' in boot
assert 'printf("%s", bootmsg);' in boot
assert 'Press ESC for boot menu.' in boot
assert 'Press configured key for boot menu.' in boot

# Dell-style boot controls are optional: neither the newer F12 convention
# nor the read-only F2 information view may alter legacy machine defaults.
assert 'WHP_DELL_MODEL_FILE "opt/org.seabios/whp-dell-model"' in boot
assert 'WHP_DELL_POST_UI_FILE "opt/org.seabios/whp-dell-post-ui"' in boot
assert 'WHP_DELL_F2_SCANCODE 0x3c' in boot
assert 'WHP_DELL_F12_SCANCODE 0x86' in boot
assert 'romfile_loadbool(WHP_DELL_POST_UI_FILE, 0)' in boot
assert 'dell_model >= 1 && dell_model <= 3' in boot
assert '!romfile_find("etc/boot-menu-key")' in boot
assert 'if (dell_ui && scan_code == WHP_DELL_F2_SCANCODE' in boot
assert 'if (menukey == WHP_DELL_F12_SCANCODE' in boot
assert 'This is not an original Dell BIOS Setup utility.' in boot
assert 'F12 opens SeaBIOS' not in boot
assert 'opt/org.seabios/whp-dell-post-ui,string=1' in docs
assert 'a separate opt-in' in docs

# F9: 0x43 - 0x3a == 9; the QEMU string option must carry the *byte*
# C (0x43), rather than the ASCII characters '0x43'.
assert 0x43 - 0x3A == 9
assert "name=etc/boot-menu-key,string=C" in docs
assert "string=0x43" in docs
assert r"printf '\103' > f9-boot-key.bin" in docs
assert "not BIOS Setup" in docs

print("SeaBIOS F9 boot-menu key and prompt: source contract verified")
