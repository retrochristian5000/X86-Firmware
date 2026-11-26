#!/usr/bin/env python3
"""Run the real-mode ISA PnP BIOS regression using an experimental ROM."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", required=True, type=Path)
    parser.add_argument("--bios", required=True, type=Path,
                        help="SeaBIOS with CONFIG_PNPBIOS_RUNTIME=y")
    parser.add_argument("--assembler", default="as")
    parser.add_argument("--linker", default="ld")
    args = parser.parse_args()
    source = Path(__file__).with_suffix(".S").resolve()

    with tempfile.TemporaryDirectory(prefix="isa-pnp-test-") as tmp:
        obj = Path(tmp) / "test.o"
        disk = Path(tmp) / "test.img"
        subprocess.run([args.assembler, "--32", "-o", str(obj), str(source)],
                       check=True, timeout=30)
        subprocess.run([args.linker, "-m", "elf_i386", "-Ttext", "0x7c00",
                        "--oformat", "binary", "-o", str(disk), str(obj)],
                       check=True, timeout=30)
        sector = disk.read_bytes()
        if len(sector) != 512 or sector[-2:] != b"\x55\xaa":
            raise SystemExit("Invalid ISA PnP test boot sector")
        with disk.open("r+b") as image:
            image.truncate(1440 * 1024)

        result = subprocess.run(
            [str(args.qemu.resolve()), "-M", "pc,acpi=off", "-accel", "tcg",
             "-nodefaults", "-vga", "none",
             "-bios", str(args.bios.resolve()), "-display", "none",
             "-serial", "none", "-monitor", "none", "-no-reboot",
             "-drive", f"if=floppy,format=raw,readonly=on,file={disk}",
             "-boot", "a", "-device", "isa-debug-exit,iobase=0xf4,iosize=1"],
            check=False, timeout=60,
        )
    if result.returncode == 0x21:
        print("ISA PnP real-mode BIOS tests passed")
        return 0
    raise SystemExit(f"ISA PnP BIOS test failed: {result.returncode}")


if __name__ == "__main__":
    raise SystemExit(main())
