#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Exercise CPU-profile selection and incremental rebuilds without a ROM."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CPUProfileTests(unittest.TestCase):
    def test_profile_stamp(self):
        make = shutil.which("gmake") or shutil.which("make")
        cc = shutil.which("gcc") or shutil.which("clang")
        if not make or not cc:
            self.skipTest("GNU make and an x86-capable compiler are required")

        # Test the real recipe, not an independently maintained copy.
        source = (ROOT / "Makefile").read_text(encoding="utf-8")
        start = source.index("profile-quote =")
        end = source.index("\n$(OUT)%.s:", start)
        with tempfile.TemporaryDirectory(prefix="seabios-cpu-") as tmp:
            work = Path(tmp)
            shutil.copyfile(ROOT / "scripts/cpu-profile.mk",
                            work / "cpu-profile.mk")
            (work / "Makefile").write_text(
                "OUT=out/\nQ=@\nconfig-only=n\n"
                "CFLAGS16=-m16\nCFLAGS32SEG=-m32\n"
                "CFLAGS32FLAT=-m32\nCFLAGSWHOLE=\n"
                "include cpu-profile.mk\n"
                "all: $(OUT)sample.o\n.PHONY: all FORCE\n"
                + source[start:end] +
                "\n$(OUT)sample.o: $(OUT)bios-cpu.profile sample.c\n"
                "\t$(CC) -m32 $(BIOS_CPU_CFLAGS) -Os -ffreestanding "
                "-Werror -c sample.c -o $@\n"
                "\t@echo built >> log\n", encoding="utf-8")
            (work / "sample.c").write_text(
                "unsigned probe(unsigned x) { return x + 1; }\n",
                encoding="utf-8")

            def build(cpu, compiler=cc, *flags):
                return subprocess.run(
                    [make, "--warn-undefined-variables", "BIOS_CPU=" + cpu,
                     "CC=" + compiler, *flags],
                    cwd=work, capture_output=True, text=True)

            for cpu, count in (
                ("i386", 1), ("i386", 1),
                ("pentium", 2), ("pentium", 2),
                ("pentium-mmx", 3), ("pentium-mmx", 3),
                ("i386", 4),
            ):
                result = build(cpu)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)
                self.assertEqual(len((work / "log").read_text().splitlines()),
                                 count)
                stamp = (work / "out/bios-cpu.profile").read_text()
                self.assertNotIn("undefined variable", result.stderr)
                self.assertEqual(stamp.splitlines()[0], "BIOS_CPU=" + cpu)
                self.assertEqual(sorted(p.name for p in (work / "out").iterdir()),
                                 ["bios-cpu.profile", "sample.o"])
                self.assertIn("-march=" + cpu, stamp)
                if cpu != "i386":
                    for flag in ("-mno-mmx", "-mno-sse", "-mno-sse2"):
                        self.assertIn(flag, stamp)
            result = build("i386", cc, "CFLAGS32FLAT=-m32 -Oz")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertNotIn("undefined variable", result.stderr)
            self.assertEqual(len((work / "log").read_text().splitlines()), 5)
            self.assertIn("CFLAGS32FLAT=-m32 -Oz",
                          (work / "out/bios-cpu.profile").read_text())
            self.assertNotEqual(build("i686").returncode, 0)
            self.assertNotEqual(build("pentium-mmx", "false").returncode, 0)


if __name__ == "__main__":
    unittest.main()
