#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Compile the real shadow handoff with the firmware register ABI."""

from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/fw/shadow.c").read_text(encoding="utf-8")
start = source.index("#define BIOS_SRC_OFFSET")
end = source.index("static void\nmake_bios_readonly_intel", start)
probe = """
#include "fw/shadow-umb.h"
#define SYMBOL(sym) ({ extern char sym[]; (u32)sym; })
#define memcpy __builtin_memcpy
void pci_ioconfig_writeb(u16 bdf, u32 addr, u8 val);
u8 pci_ioconfig_readb(u16 bdf, u32 addr);
""" + source[start:end] + """
void shadow_probe(u16 bdf, u32 pam0)
{
    make_bios_writable_intel(bdf, pam0);
}
"""

with tempfile.TemporaryDirectory(prefix="seabios-shadow-") as tmp:
    work = Path(tmp)
    unit = work / "shadow.c"
    unit.write_text(probe, encoding="utf-8")
    for name in ("gcc", "clang"):
        cc = shutil.which(name)
        if not cc:
            raise SystemExit("Required compiler not found: " + name)
        target = ["--target=i386-none-elf"] if name == "clang" else []
        for cpu in ("i386", "pentium-mmx"):
            subprocess.run(
                [cc, *target, "-m32", "-march=" + cpu, "-mregparm=3",
                 "-ffreestanding", "-fno-pie", "-fno-stack-protector",
                 "-fomit-frame-pointer", "-Os", "-Wall", "-Wextra",
                 "-Werror", "-DMODE16=0", "-DMODESEGMENT=0",
                 "-I" + str(ROOT / "src"), "-c", str(unit),
                 "-o", str(work / "shadow.o")], check=True)
            print(name + " " + cpu + ": shadow handoff compiled")
