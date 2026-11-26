#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Stable firmware identity independent of host and tool descriptions."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parent / "buildversion.py"


class VersionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.header = self.root / "version.h"

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root,
                                       text=True).strip()

    def generate(self, tool="false", extra="-whp", **env):
        result = subprocess.run(
            [sys.executable, str(SCRIPT), "-e", extra, "-t", tool,
             str(self.header)], cwd=self.root, env=dict(os.environ, **env),
            capture_output=True, text=True, check=True)
        return self.header.read_text(), result.stdout

    def test_archive_is_stable_and_does_not_embed_tools(self):
        (self.root / ".version").write_text("1.16.3\n")
        first, log = self.generate(tool="git", HOSTNAME="build-one",
                                   TZ="UTC", SOURCE_DATE_EPOCH="1")
        stamp = self.header.stat().st_mtime_ns
        second, _ = self.generate(tool="false", HOSTNAME="long-build-host",
                                  TZ="Pacific/Honolulu", SOURCE_DATE_EPOCH="2")
        self.assertEqual(first, second)
        self.assertEqual(stamp, self.header.stat().st_mtime_ns)
        self.assertIn('#define BUILD_VERSION "1.16.3-whp"', first)
        self.assertIn('#define BUILD_TOOLS "see build log"', first)
        self.assertIn("git version", log)
        self.assertNotIn("git version", first)

    def test_git_identity_is_fixed_width_and_dirty_is_explicit(self):
        self.git("init", "-q")
        self.git("config", "user.name", "Test")
        self.git("config", "user.email", "test@example.invalid")
        tracked = self.root / "tracked"
        tracked.write_text("one")
        self.git("add", "tracked")
        self.git("commit", "-qm", "test")
        sha = self.git("rev-parse", "HEAD")
        first, log = self.generate()
        self.assertIn('#define BUILD_VERSION "git-' + sha[:12] + '-whp"', first)
        self.assertIn(sha, log)
        self.git("tag", "a-much-longer-local-tag")
        self.git("config", "core.abbrev", "20")
        self.assertEqual(first, self.generate()[0])
        tracked.write_text("two")
        self.assertIn('git-' + sha[:12] + '-dirty-whp', self.generate()[0])

    def test_unknown_archive_and_c_string_escaping(self):
        first, _ = self.generate(extra='-"\\?\n')
        self.assertIn(r'#define BUILD_VERSION "unknown-\042\134\077\012"', first)


if __name__ == "__main__":
    unittest.main()
