#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check extra-stack membership and the stack-usage reporting helpers."""

from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/stacks.c").read_text(encoding="utf-8")
start = source.index("int\non_extra_stack(")
end = source.index("// Switch to the extra stack", start)
config = (ROOT / "src/config.h").read_text(encoding="utf-8")
size = re.search(r"^#define BUILD_EXTRA_STACK_SIZE\s+(0x[0-9a-fA-F]+|\d+)",
                 config, re.MULTILINE)
assert size is not None

STUB = r"""
#include <stdint.h>
#include <assert.h>

typedef uint32_t u32;
static int mode16 = 1;
static unsigned ss = 0x9000;
static u32 esp;
#define MODE16 mode16
#define SEG_LOW 0x9000
#define GET_SEG(reg) ss
#define ExtraStack 0x4000u
static u32 getesp(void) { return esp; }
"""

CHECKS = r"""
int main(void)
{
    esp = ExtraStack - 1;
    assert(!on_extra_stack());
    esp = ExtraStack;
    assert(!on_extra_stack());
    esp = ExtraStack + 1;
    assert(on_extra_stack());
    esp = ExtraStack + BUILD_EXTRA_STACK_SIZE / 2;
    assert(on_extra_stack());
    esp = ExtraStack + BUILD_EXTRA_STACK_SIZE;
    assert(on_extra_stack());
    esp++;
    assert(!on_extra_stack());
    esp = UINT32_MAX;
    assert(!on_extra_stack());
    esp = 0;
    assert(!on_extra_stack());

    esp = ExtraStack + 64;
    ss++;
    assert(!on_extra_stack());
    ss = SEG_LOW;
    mode16 = 0;
    assert(!on_extra_stack());
    return 0;
}
"""

cc = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
if not cc:
    raise SystemExit("native C compiler required")
with tempfile.TemporaryDirectory(prefix="seabios-stack-bounds-") as temp:
    program = Path(temp) / "stack-bounds.c"
    binary = Path(temp) / "stack-bounds"
    program.write_text(
        "#define BUILD_EXTRA_STACK_SIZE " + size.group(1) + "\n"
        + STUB + source[start:end] + CHECKS, encoding="utf-8",
    )
    subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    str(program), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

# Mimic the GNU objdump format consumed by checkstack.py.
assembly = (
    "00000000 <caller>:\n"
    "   0:\t66 50\tpushl  %eax\n"
    "   2:\t66 ff d0\tcalll  *%eax\n"
    "   5:\tff d0\tcallw  *%ax\n"
    "   7:\t66 c3\tretl\n"
    "00000010 <hop_caller>:\n"
    "  10:\t66 50\tpushl  %eax\n"
    "  12:\t66 e8 00 00 00 00\tcalll  20 <__stack_hop.constprop.0>\n"
    "  18:\t66 c3\tretl\n"
    "00000020 <__stack_hop.constprop.0>:\n"
    "  20:\t66 83 ec 40\tsubl   $0x40,%esp\n"
    "  24:\t66 c3\tretl\n"
)
report = subprocess.run(
    [sys.executable, str(ROOT / "scripts/checkstack.py")],
    input=assembly, text=True, capture_output=True, check=True,
).stdout
assert "caller[4,8]:" in report, report
assert "hop_caller[4,8]:" in report, report
assert re.search(r"<indirect>\s+\[8\+0,8\]", report), report
assert re.search(r"<indirect>\s+\[6\+0,6\]", report), report
print("SeaBIOS stack membership: 10 cases passed; stack report checks passed")
