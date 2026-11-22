#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

import contextlib
import io
import os
import tempfile
import unittest
from unittest import mock

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



class RomCapacityTests(unittest.TestCase):
    def run_check(self, actual_size, linked_size=None, rom_kib=128):
        # Build a synthetic linked binary with a valid reset vector at the
        # architectural address, and stub only the objdump text parser.
        if linked_size is None:
            linked_size = actual_size
        end = layoutrom.BUILD_BIOS_ADDR + layoutrom.BUILD_BIOS_SIZE
        start = end - linked_size
        symbols = {
            'code32flat_start': StubSymbol(start),
            'code32flat_end': StubSymbol(end),
            'code32init_end': StubSymbol(end - 4096),
            'reset_vector': StubSymbol(checkrom.RESET_VECTOR_ADDR),
        }
        image = bytearray(actual_size)
        resetoff = checkrom.RESET_VECTOR_ADDR - start
        if 0 <= resetoff <= len(image) - len(checkrom.RESET_VECTOR_DATA):
            image[resetoff:resetoff + len(checkrom.RESET_VECTOR_DATA)] = (
                checkrom.RESET_VECTOR_DATA)

        with tempfile.TemporaryDirectory() as directory:
            objdump = os.path.join(directory, 'rom.objdump')
            raw = os.path.join(directory, 'bios.bin.raw')
            output = os.path.join(directory, 'bios.bin.prep')
            with open(objdump, 'w', encoding='ascii') as f:
                f.write('')
            with open(raw, 'wb') as f:
                f.write(image)
            stdout = io.StringIO()
            argv = ['checkrom.py', objdump, str(rom_kib), raw, output]
            with mock.patch.object(checkrom.layoutrom, 'parseObjDump',
                                   return_value=(None, symbols)):
                with mock.patch.object(checkrom.sys, 'argv', argv):
                    with contextlib.redirect_stdout(stdout):
                        try:
                            checkrom.main()
                            failed = False
                        except SystemExit as error:
                            self.assertEqual(error.code, 1)
                            failed = True
            exists = os.path.exists(output)
            binary = None
            if exists:
                with open(output, 'rb') as f:
                    binary = f.read()
            return failed, stdout.getvalue(), exists, binary

    def test_exact_128k_limit_is_accepted(self):
        failed, report, exists, image = self.run_check(128 * 1024)
        self.assertFalse(failed)
        self.assertTrue(exists)
        self.assertEqual(len(image), 128 * 1024)
        self.assertIn('Free: 0', report)

    def test_128k_limit_minus_64_is_accepted(self):
        failed, report, exists, image = self.run_check(131008)
        self.assertFalse(failed)
        self.assertTrue(exists)
        self.assertEqual(len(image), 128 * 1024)
        self.assertIn('Free: 64', report)

    def test_128k_limit_plus_64_is_rejected(self):
        failed, report, exists, image = self.run_check(131136)
        self.assertTrue(failed)
        self.assertIn("ROM doesn't fit (131136 > 131072)", report)
        self.assertIn('64 bytes over', report)
        self.assertFalse(exists)
        self.assertIsNone(image)

    def test_128k_limit_plus_160_is_rejected(self):
        failed, report, exists, image = self.run_check(131232)
        self.assertTrue(failed)
        self.assertIn("ROM doesn't fit (131232 > 131072)", report)
        self.assertIn('160 bytes over', report)
        self.assertFalse(exists)
        self.assertIsNone(image)

    def test_mismatched_image_is_not_misreported_as_overfill(self):
        failed, report, exists, _ = self.run_check(
            131232, linked_size=131072)
        self.assertTrue(failed)
        self.assertIn('Unknown extra data', report)
        self.assertNotIn("ROM doesn't fit", report)
        self.assertFalse(exists)

    def test_auto_rom_size_selects_256k_for_overflow(self):
        failed, report, exists, image = self.run_check(131232, rom_kib=0)
        self.assertFalse(failed)
        self.assertTrue(exists)
        self.assertEqual(len(image), 256 * 1024)
        self.assertIn('of 256KiB rom', report)


if __name__ == '__main__':
    unittest.main()
