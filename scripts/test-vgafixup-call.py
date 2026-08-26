#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
fixup = ROOT / 'scripts' / 'vgafixup.py'

source = '''\
calll foo
calll *%eax
call bar
'''
expected = '''\
pushw %ax ; callw foo
pushw %ax ; callw *%eax
pushw %ax ; callw bar
'''

with tempfile.TemporaryDirectory(prefix='vgafixup-call-') as tmp:
    tmp = Path(tmp)
    src = tmp / 'input.s'
    out = tmp / 'output.s'
    src.write_text(source, encoding='utf-8')
    subprocess.run([sys.executable, str(fixup), str(src), str(out)], check=True)
    got = out.read_text(encoding='utf-8')

assert got == expected, f'unexpected call rewrite:\n{got}'
assert 'callwl' not in got, 'vgafixup generated invalid callwl mnemonic'

print('VGA call fixup: verified')
