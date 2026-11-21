#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

import contextlib
import io
import unittest

import checkrom
import layoutrom


class StubSection:
    def __init__(self, name, finalloc, finalsegloc, fileid=None,
                 size=0, category=None):
        self.name = name
        self.finalloc = finalloc
        self.finalsegloc = finalsegloc
        self.fileid = fileid
        self.size = size
        self.category = category


class StubSymbol:
    def __init__(self, offset):
        self.offset = offset


class LinkerScriptTests(unittest.TestCase):
    def test_output_sections_are_sorted_by_link_address(self):
        high = StubSection('.text.high', 0x2200, 0x220)
        low = StubSection('.text.low', 0x1100, 0x110)

        script = layoutrom.outSections([high, low], useseg=1)

        self.assertLess(script.index('.text.low'), script.index('.text.high'))

    def test_relative_sections_use_absolute_link_addresses(self):
        section = StubSection('.text.fixed', 0x1234, 0x234, '16')

        script = layoutrom.outRelSections([section])

        self.assertIn('. = ABSOLUTE(0x1234) ;', script)


class RomSizeReportTests(unittest.TestCase):
    def test_only_reachable_rom_sections_are_ranked(self):
        sections = [
            StubSection('.text.small', 0x1000, 0, '32flat', 8, '32init'),
            StubSection('.text.large', 0x2000, 0, '32flat', 60, '32init'),
            StubSection('.rodata.table', 0x3000, 0, '16', 40, '16'),
            StubSection('.bss.scratch', 0x4000, 0, '32flat', 900, '32init'),
            StubSection('.text.unused', None, None, '32flat', 500, '32init'),
        ]
        positions = [section.finalloc for section in sections]
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            layoutrom.reportRomSectionSizes(sections, limit=2)

        report = output.getvalue()
        self.assertIn('ROM size audit: retained input sections', report)
        self.assertIn('16', report)
        self.assertIn('32init', report)
        self.assertRegex(report, r'32init\s+68\n')
        self.assertRegex(report, r'16\s+40\n')
        self.assertNotIn('.bss.scratch', report)
        self.assertNotIn('.text.unused', report)
        self.assertLess(report.index('.text.large'),
                        report.index('.rodata.table'))
        self.assertNotIn('.text.small', report)
        self.assertEqual(positions, [section.finalloc for section in sections])


class ResetVectorTests(unittest.TestCase):
    def test_reset_vector_must_be_at_architectural_address(self):
        symbols = {'reset_vector': StubSymbol(0xfffe0)}
        rawdata = b'\xea\x5b\xe0\x00\xf0' + b'\0' * 27

        with self.assertRaisesRegex(ValueError, '0xffff0'):
            checkrom.checkResetVector(rawdata, 0xfffe0, symbols)


if __name__ == '__main__':
    unittest.main()
