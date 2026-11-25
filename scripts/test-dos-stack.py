#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise DOS-style real-mode interrupt stacks in an actual BIOS ROM."""

import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--qemu", default="qemu-system-i386")
parser.add_argument("--bios", type=Path, required=True)
parser.add_argument("--data-dir", type=Path)
args = parser.parse_args()
bios = args.bios.resolve(strict=True)
source = Path(__file__).with_suffix(".asm")

with tempfile.TemporaryDirectory(prefix="seabios-dos-stack-") as temp:
    directory = Path(temp)
    sector = directory / "boot.bin"
    subprocess.run(["nasm", "-f", "bin", str(source), "-o", str(sector)],
                   check=True)
    boot = sector.read_bytes()
    assert len(boot) == 512 and boot[-2:] == b"\x55\xaa"
    image = directory / "disk.img"
    with image.open("wb") as disk:
        disk.write(boot)
        disk.write(b"\xcd\xab")
        disk.truncate(1440 * 1024)

    for interface, order in (("floppy", "a"), ("ide", "c")):
        command = [
            args.qemu, "-bios", str(bios), "-machine", "pc,accel=tcg",
            "-m", "32M", "-smp", "1", "-display", "none", "-vga", "none",
            "-serial", "none", "-monitor", "none", "-net", "none",
            "-no-reboot", "-debugcon", "stdio", "-device",
            "isa-debug-exit,iobase=0xf4,iosize=4",
            "-drive", f"file={image},format=raw,if={interface},snapshot=on",
            "-boot", order,
        ]
        if args.data_dir:
            command.extend(["-L", str(args.data_dir.resolve())])
        result = subprocess.run(command, capture_output=True, timeout=20)
        if result.returncode != 33 or result.stdout != b"P":
            raise SystemExit(f"{interface}: BIOS stack probe failed: "
                             f"exit={result.returncode}, "
                             f"debug={result.stdout!r}, "
                             f"stderr={result.stderr!r}")
        print(f"{interface}: DOS-style stack and E820 checks passed")
