#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

import unittest

import checkrom
import layoutrom


class StubSection:
    def __init__(self, name, finalloc, finalsegloc, fileid=None):
        self.name = name
        self.finalloc = finalloc
        self.finalsegloc = finalsegloc
        self.fileid = fileid


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


class ResetVectorTests(unittest.TestCase):
    def test_reset_vector_must_be_at_architectural_address(self):
        symbols = {'reset_vector': StubSymbol(0xfffe0)}
        rawdata = b'\xea\x5b\xe0\x00\xf0' + b'\0' * 27

        with self.assertRaisesRegex(ValueError, '0xffff0'):
            checkrom.checkResetVector(rawdata, 0xfffe0, symbols)


if __name__ == '__main__':
    unittest.main()
