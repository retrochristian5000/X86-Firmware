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
                 size=0, category=None, align=1):
        self.name = name
        self.finalloc = finalloc
        self.finalsegloc = finalsegloc
        self.fileid = fileid
        self.size = size
        self.category = category
        self.align = align


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


class SectionPlacementTests(unittest.TestCase):
    def test_fixed_gap_records_aligned_location(self):
        first = StubSection('.fixedaddr.ff00', None, None, '16', size=3)
        reset = StubSection('.fixedaddr.fff0', None, None, '16', size=16)
        code = StubSection('.text.aligned', None, None, '16', size=8,
                           align=16)
        with contextlib.redirect_stdout(io.StringIO()):
            layoutrom.fitSections([first, reset], [code])
        self.assertEqual(code.finalsegloc, 0xff10)
        self.assertEqual(code.finalloc, 0xfff10)
        self.assertEqual(reset.finalloc, 0xffff0)
        self.assertIn('.text.aligned 0xff10 :',
                      layoutrom.outSections([code], useseg=1))
        self.assertIn('ABSOLUTE(0xfff10)', layoutrom.outRelSections([code]))

    def test_fixed_gap_with_padding_cannot_overlap_next_entry(self):
        first = StubSection('.fixedaddr.ff00', None, None, '16', size=3)
        reset = StubSection('.fixedaddr.fff0', None, None, '16', size=16)
        code = StubSection('.text.too_large', None, None, '16',
                           size=0xe1, align=16)
        with contextlib.redirect_stdout(io.StringIO()):
            layoutrom.fitSections([first, reset], [code])
        self.assertIsNone(code.finalloc)

    def test_overlapping_fixed_sections_are_rejected(self):
        first = StubSection('.fixedaddr.ffe0', None, None, '16', size=17)
        reset = StubSection('.fixedaddr.fff0', None, None, '16', size=16)
        with self.assertRaisesRegex(SystemExit, 'overlaps'):
            layoutrom.fitSections([first, reset], [])

    def test_negative_layout_start_rounds_down(self):
        code = StubSection('.text.code', None, None, '32flat',
                           size=17, align=16)
        start, alignment = layoutrom.setSectionsStart([code], 16)
        self.assertEqual((start, alignment), (-16, 16))
        self.assertLessEqual(code.finalloc + code.size, 16)

    def test_segment_offsets_and_input_alignment_preserved(self):
        sections = [
            StubSection('.text.first', None, None, '16', size=3, align=4),
            StubSection('.text.second', None, None, '16', size=7, align=16),
        ]
        start, alignment = layoutrom.setSectionsStart(
            sections, 0xfff00, segoffset=layoutrom.BUILD_BIOS_ADDR)
        self.assertEqual(alignment, 16)
        self.assertEqual(start, sections[0].finalloc)
        for section in sections:
            self.assertEqual(section.finalloc % section.align, 0)
            self.assertEqual(section.finalsegloc,
                             section.finalloc - layoutrom.BUILD_BIOS_ADDR)
        self.assertLessEqual(sections[0].finalloc + sections[0].size,
                             sections[1].finalloc)
        self.assertLessEqual(sections[1].finalloc + sections[1].size, 0xfff00)


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


class RomStringCodeTests(unittest.TestCase):
    def test_report_excludes_unreferenced_helpers_and_non_string_code(self):
        sections = [
            StubSection('.text.strlen', 0x1000, 0, '16', 34, '16'),
            StubSection('.text.memcpy', 0x2000, 0, '32flat', 27, '32flat'),
            StubSection('.text.memmove', None, None, '32flat', 90, '32flat'),
            StubSection('.text.not_a_helper', 0x3000, 0, '16', 500, '16'),
        ]
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            layoutrom.reportRomStringCode(sections)
        report = output.getvalue()
        self.assertIn('.text.strlen', report)
        self.assertIn('.text.memcpy', report)
        self.assertNotIn('.text.memmove', report)
        self.assertNotIn('.text.not_a_helper', report)
        self.assertLess(report.index('.text.strlen'), report.index('.text.memcpy'))


class RomStringReportTests(unittest.TestCase):
    def test_retained_read_only_sections_are_grouped_by_code_mode(self):
        sections = [
            StubSection('.rodata.str1.1.nomerge', 0x1000, 0, '16', 24, '16'),
            StubSection('.rodata.flags', 0x2000, 0, '32flat', 12, '32init'),
            StubSection('.rodata.str1.1.nomerge', 0x3000, 0, '32flat',
                        16, '32init'),
            StubSection('.rodata.unused', None, None, '32flat', 90, '32init'),
            StubSection('.text.boot', 0x4000, 0, '16', 500, '16'),
        ]
        before = [section.finalloc for section in sections]
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            layoutrom.reportRomStringSections(sections, limit=2)

        report = output.getvalue()
        self.assertIn('ROM string audit: retained read-only sections', report)
        self.assertRegex(report, r'16\s+literal-pool\s+24\n')
        self.assertRegex(report, r'32init\s+literal-pool\s+16\n')
        self.assertRegex(report, r'32init\s+other-rodata\s+12\n')
        self.assertNotIn('.rodata.unused', report)
        self.assertNotIn('.text.boot', report)
        self.assertIn('.rodata.str1.1.nomerge', report)
        self.assertEqual(before, [section.finalloc for section in sections])


class RomAlignmentReportTests(unittest.TestCase):
    def test_start_padding_identifies_highest_aligned_sections(self):
        sections = [
            StubSection('.text.high', 0x1000, 0, '16', align=64),
            StubSection('.data.small', 0x2000, 0, '32flat', align=4),
            StubSection('.rodata.high', 0x3000, 0, '32flat', align=64),
        ]
        positions = [section.finalloc for section in sections]
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            layoutrom.reportRomStartAlignment(
                sections, 0xe003f, 0xe0000, 64, 12, 0)

        report = output.getvalue()
        self.assertIn('boundary=64 padding=63 relocations=12 multiboot=0',
                      report)
        self.assertIn('alignment source: 16     .text.high', report)
        self.assertIn('alignment source: 32flat .rodata.high', report)
        self.assertNotIn('.data.small', report)
        self.assertEqual(positions, [section.finalloc for section in sections])

    def test_no_padding_when_link_start_is_already_aligned(self):
        section = StubSection('.text.entry', 0x1000, 0, '16', align=16)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            layoutrom.reportRomStartAlignment(
                [section], 0xe0100, 0xe0100, 16, 0, 12)
        self.assertIn('boundary=16 padding=0 relocations=0 multiboot=12',
                      output.getvalue())


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
