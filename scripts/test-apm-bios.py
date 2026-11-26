#!/usr/bin/env python3
"""Test real-mode APM calls against a built SeaBIOS image using QEMU."""

import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def make_boot_sector():
    # Establish a real-mode stack; no dependency on the guest disk contents.
    code = bytearray.fromhex("fa 31 c0 8e d0 bc 00 7c 8e d8 fb")
    checks = []

    def call(ax, bx=0, cx=0):
        code.extend(struct.pack("<BHBHBH", 0xb8, ax, 0xbb, bx, 0xb9, cx))
        code.extend(b"\xcd\x15")

    def check(opcode, label):
        # On the expected condition, skip the failure exit sequence.
        value = 0x20 + len(checks)
        checks.append(label)
        code.extend(bytes((opcode, 5, 0xb0, value, 0xe6, 0xf4, 0xf4)))

    def success(label):
        check(0x73, label)  # JNC

    def error(ah, label):
        check(0x72, label + " carry")  # JC
        code.extend(bytes((0x80, 0xfc, ah)))  # CMP AH, imm8
        check(0x74, label + " code")  # JE

    def ax_is(value, label):
        code.extend(struct.pack("<BH", 0x3d, value))
        check(0x74, label)

    call(0x5300)
    success("installation")
    ax_is(0x0102, "APM 1.2 advertised")
    code.extend(bytes.fromhex("81 fb 4d 50"))
    check(0x74, "PM signature")
    call(0x5300, 2)
    error(0x09, "invalid BIOS ID")
    call(0x5301)
    success("real-mode connect")
    call(0x530e, 0, 0x0101)
    success("APM 1.1 negotiation")
    ax_is(0x0101, "negotiated version")
    call(0x530e, 0, 0x0102)
    success("APM 1.2 negotiation")
    ax_is(0x0102, "negotiated version")
    call(0x5307, 1, 1)
    error(0x60, "unsupported standby")
    call(0x5307, 1, 2)
    error(0x60, "unsupported suspend")
    call(0x5307, 1, 6)
    error(0x0a, "invalid power state")
    call(0x5308, 1, 2)
    error(0x0a, "invalid enable value")
    call(0x530a, 0x8001)
    error(0x09, "absent desktop battery")
    call(0x530a, 1)
    success("desktop power status")
    code.extend(bytes.fromhex("81 fb ff 01"))
    check(0x74, "AC online")
    call(0x530f, 1, 2)
    error(0x0a, "invalid engagement value")
    call(0x5310)
    success("capabilities")
    code.extend(bytes.fromhex("83 f9 00"))
    check(0x74, "no advertised sleep capabilities")
    call(0x5304)
    success("disconnect")
    code.extend(bytes.fromhex("b0 10 e6 f4 f4 eb fd"))
    if len(code) > 510:
        raise ValueError("APM test exceeds boot-sector capacity")
    return code.ljust(510, b"\x00") + b"\x55\xaa", checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", required=True, type=Path)
    parser.add_argument("--bios", required=True, type=Path)
    args = parser.parse_args()
    sector, checks = make_boot_sector()
    with tempfile.TemporaryDirectory(prefix="apm-bios-test-") as tmp:
        disk = Path(tmp) / "test.img"
        with disk.open("wb") as f:
            f.write(sector)
            f.truncate(1440 * 1024)
        result = subprocess.run(
            [str(args.qemu.resolve()), "-M", "pc", "-accel", "tcg",
             "-bios", str(args.bios.resolve()), "-display", "none",
             "-serial", "none", "-monitor", "none", "-no-reboot",
             "-drive", f"if=floppy,format=raw,readonly=on,file={disk}",
             "-boot", "a", "-device", "isa-debug-exit,iobase=0xf4,iosize=1"],
            timeout=60, check=False,
        )
    if result.returncode == 0x21:
        print("APM real-mode regression passed")
        return 0
    index = (result.returncode - 1) // 2 - 0x20
    if result.returncode > 0 and result.returncode % 2 and 0 <= index < len(checks):
        raise SystemExit("APM regression failed: " + checks[index])
    raise SystemExit(f"Unexpected QEMU exit status: {result.returncode}")


if __name__ == "__main__":
    raise SystemExit(main())
