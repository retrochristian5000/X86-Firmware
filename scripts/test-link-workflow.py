#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify incremental scheduling of the actual SeaBIOS linker rules."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LinkWorkflowTests(unittest.TestCase):
    def test_parallel_and_incremental_link_jobs(self):
        source = (ROOT / 'Makefile').read_text(encoding='utf-8')
        start = source.index('$(OUT)autoversion.h:')
        end = source.index('$(OUT)bios.bin.prep:', start)
        rules = source[start:end]
        with tempfile.TemporaryDirectory(prefix='seabios-link-') as temp:
            work = Path(temp)
            for directory in ('out', 'src', 'scripts'):
                (work / directory).mkdir()
            for name in ('ccode32flat.o', 'ccode16.o', 'code32seg.o',
                         'romlayout.o', 'autoconf.h'):
                (work / 'out' / name).touch()
            (work / 'src/version.c').touch()
            (work / 'scripts/buildversion.py').write_text(
                'import pathlib, sys\n'
                'pathlib.Path(sys.argv[-1]).touch()\n', encoding='utf-8')
            (work / 'scripts/layoutprofiles.py').touch()
            (work / 'scripts/layoutrom.py').write_text(
                'import pathlib, sys\n'
                'print("LAYOUT")\n'
                'for name in sys.argv[-3:]: pathlib.Path(name).touch()\n',
                encoding='utf-8')
            (work / 'tool.py').write_text(
                'import pathlib, sys\n'
                'args = sys.argv[1:]\n'
                'if "-o" in args:\n'
                '    output = pathlib.Path(args[args.index("-o") + 1])\n'
                '    print("LINK " + str(output) if "-c" not in args '
                'else "COMPILE " + str(output))\n'
                '    output.touch()\n',
                encoding='utf-8')
            makefile = (
                'OUT = out/\n'
                'KCONFIG_AUTOHEADER = autoconf.h\n'
                f'PYTHON = {sys.executable}\n'
                'CC = $(PYTHON) tool.py\n'
                'LD = $(PYTHON) tool.py\n'
                'OBJDUMP = $(PYTHON) tool.py\n'
                'all: $(OUT)rom.o\n'
                '.PHONY: FORCE\n'
                '%.noexec.o: %.o\n'
                '\tcp $< $@\n'
                + rules
            )
            (work / 'Makefile').write_text(makefile, encoding='utf-8')

            def build():
                env = dict(os.environ)
                env.pop('MAKEFLAGS', None)
                env.pop('MFLAGS', None)
                return subprocess.run(
                    ['make', '-rR', '-j4', 'all'], cwd=work, env=env,
                    text=True, capture_output=True, check=True,
                ).stdout

            def link_count(output):
                return sum(line.startswith('LINK ')
                           for line in output.splitlines())

            first = build()
            self.assertEqual(first.splitlines().count('LAYOUT'), 1, first)
            self.assertEqual(link_count(first), 5, first)
            noop = build()
            self.assertEqual(noop.splitlines().count('LAYOUT'), 0, noop)
            self.assertEqual(link_count(noop), 0, noop)

            # Invalidate only the script generator, not the compiled objects.
            time.sleep(0.02)
            (work / 'scripts/layoutrom.py').touch()
            layout = build()
            self.assertEqual(layout.splitlines().count('LAYOUT'), 1, layout)
            self.assertEqual(link_count(layout), 3, layout)
            self.assertNotIn('COMPILE out/version.o', layout)

            # An interrupted or partial build can lose any generated script.
            for name in ('romlayout16.lds', 'romlayout32seg.lds',
                         'romlayout32flat.lds'):
                (work / 'out' / name).unlink()
                recovered = build()
                self.assertEqual(recovered.splitlines().count('LAYOUT'), 1, recovered)
                self.assertTrue((work / 'out' / name).is_file())
                self.assertEqual(link_count(recovered), 3, recovered)
                self.assertNotIn('LINK out/code16.o', recovered)
                self.assertNotIn('LINK out/code32flat.o', recovered)
                self.assertEqual(link_count(build()), 0)


if __name__ == '__main__':
    unittest.main()
