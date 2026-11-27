#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Check the real-mode compiler flag fallback without building a ROM."""

from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class InlinePolicyTests(unittest.TestCase):
    def test_stack_frame_option_does_not_disable_inlining(self):
        make = shutil.which("gmake") or shutil.which("make")
        if not make:
            self.skipTest("GNU make is required")

        source = (ROOT / "Makefile").read_text(encoding="utf-8")
        probe = source[source.index("cc-option="):
                       source.index("\n\nEXTRAVERSION")]
        start = source.index("CFLAGS16 := ")
        end = source.index("\n\n", start)
        flags = source[start:end]
        recipe = (probe + "\n"
                  "CFLAGSSEG := -Os -mregparm=3 -DMODESEGMENT=1\n"
                  "CFLAGS16_MODE := -m16\n" + flags + "\n"
                  "all:\n\t@printf '%s\\n' '$(CFLAGS16)'\n")

        # Model both outcomes of the existing diagnostic-based probe.
        cases = (("true", True), ("sh -c 'echo unsupported >&2' --", False))
        for compiler, supports_option in cases:
            with self.subTest(compiler=compiler):
                result = subprocess.run(
                    [make, "-s", "-f", "-", "CC=" + compiler],
                    input=recipe, capture_output=True, text=True, check=True)
                actual = result.stdout.split()
                self.assertNotIn("-fno-inline", actual)
                self.assertIn("-m16", actual)
                self.assertIn("-mregparm=3", actual)
                self.assertIn("-DMODE16=1", actual)
                self.assertEqual("--param" in actual, supports_option)
                self.assertEqual("large-stack-frame=4" in actual,
                                 supports_option)


if __name__ == "__main__":
    unittest.main()
