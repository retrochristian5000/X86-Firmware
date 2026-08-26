#!/usr/bin/env python3
"""Verify the optional NASM build rule produces SeaBIOS-compatible ELF32 objects."""

from __future__ import annotations

import os
import pathlib
import shutil
import struct
import subprocess
import tempfile


ROOT = pathlib.Path(__file__).resolve().parent.parent


def run_make(out_dir: pathlib.Path, nasm: str, source: pathlib.Path) -> pathlib.Path:
    target = out_dir / source.with_suffix(".o").name
    subprocess.run(
        [
            "make",
            "--no-print-directory",
            "-C",
            str(ROOT),
            f"OUT={out_dir}/",
            f"NASM={nasm}",
            str(target),
        ],
        check=True,
    )
    if not target.is_file():
        raise SystemExit(f"NASM rule did not produce {target}")
    return target


def check_elf32_i386(path: pathlib.Path) -> None:
    data = path.read_bytes()
    if len(data) < 20 or data[:4] != b"\x7fELF":
        raise SystemExit(f"NASM output is not ELF: {path}")
    if data[4] != 1 or data[5] != 1:
        raise SystemExit(f"NASM output must be little-endian ELF32: {path}")
    if struct.unpack_from("<H", data, 16)[0] != 1:
        raise SystemExit(f"NASM output must be relocatable ELF: {path}")
    if struct.unpack_from("<H", data, 18)[0] != 3:
        raise SystemExit(f"NASM output must target EM_386: {path}")


def main() -> None:
    source = ROOT / f".whp-nasm-smoke-{os.getpid()}.asm"
    source.write_text(
        "BITS 32\n"
        "SECTION .text\n"
        "GLOBAL seabios_nasm_smoke\n"
        "seabios_nasm_smoke:\n"
        "    mov eax, 0x5342494f\n"
        "    ret\n",
        encoding="utf-8",
    )

    try:
        with tempfile.TemporaryDirectory(prefix="seabios-nasm-") as tmp:
            scratch = pathlib.Path(tmp)
            fake = scratch / "fake-nasm"
            log = scratch / "fake-nasm.log"
            fake.write_text(
                "#!/bin/sh\n"
                "set -eu\n"
                ": \"${NASM_TEST_LOG:?}\"\n"
                "printf '%s\\n' \"$@\" > \"$NASM_TEST_LOG\"\n"
                "format=\n"
                "out=\n"
                "while [ \"$#\" -gt 0 ]; do\n"
                "    case \"$1\" in\n"
                "        -f) format=$2; shift 2 ;;\n"
                "        -o) out=$2; shift 2 ;;\n"
                "        *) shift ;;\n"
                "    esac\n"
                "done\n"
                "[ \"$format\" = elf32 ]\n"
                "[ -n \"$out\" ]\n"
                "mkdir -p \"$(dirname \"$out\")\"\n"
                ": > \"$out\"\n",
                encoding="utf-8",
            )
            fake.chmod(0o755)

            fake_out = scratch / "fake-out"
            env = os.environ.copy()
            env["NASM_TEST_LOG"] = str(log)
            target = fake_out / source.with_suffix(".o").name
            subprocess.run(
                [
                    "make",
                    "--no-print-directory",
                    "-C",
                    str(ROOT),
                    f"OUT={fake_out}/",
                    f"NASM={fake}",
                    str(target),
                ],
                check=True,
                env=env,
            )
            args = log.read_text(encoding="utf-8").splitlines()
            if "-f" not in args or "elf32" not in args:
                raise SystemExit(f"NASM rule did not force ELF32 output: {args!r}")
            if "-o" not in args or str(target) not in args:
                raise SystemExit(f"NASM rule did not pass the output path: {args!r}")
            if source.name not in args and str(source) not in args:
                raise SystemExit(f"NASM rule did not pass the input source: {args!r}")

            nasm = shutil.which("nasm")
            if nasm:
                real_out = scratch / "real-out"
                real_obj = run_make(real_out, nasm, source)
                check_elf32_i386(real_obj)
                ld = shutil.which("ld")
                if ld:
                    linked = scratch / "nasm-linked.o"
                    subprocess.run(
                        [ld, "-m", "elf_i386", "-r", str(real_obj), "-o", str(linked)],
                        check=True,
                    )
                    check_elf32_i386(linked)
                print(f"SeaBIOS NASM smoke object: {real_obj}")
            else:
                print("SeaBIOS NASM smoke object: real NASM unavailable; rule contract verified")
    finally:
        source.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
