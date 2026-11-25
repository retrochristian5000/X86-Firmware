#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check AHCI descriptor preparation and legacy ATAPI non-data handling."""

from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/hw/ahci.c").read_text(encoding="utf-8")
start = source.index("static int\nahci_prepare_transfer(")
end = source.index("// submit ahci command", start)
# Use the actual structure layouts and command flag definitions.
header = (ROOT / "src/hw/ahci.h").read_text(encoding="utf-8")
header = header.replace('#include "block.h" // struct drive_s', "")
header = header.replace('#include "types.h" // u32', "")
program = r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
struct drive_s { int dummy; };
""" + header + source[start:end].replace("(u32)buffer", "(u32)(uintptr_t)buffer") + r"""
int main(void)
{
    /* The flexible PRDT array has one entry, just like the firmware. */
    union {
        struct ahci_cmd_s cmd;
        uint32_t words[(sizeof(struct ahci_cmd_s) + 16) / 4];
    } storage;
    struct ahci_cmd_s *cmd = &storage.cmd;
    struct ahci_list_s list;
    void *buffer = (void *)(uintptr_t)0x10000;
    memset(&storage, 0xff, sizeof(storage));
    memset(&list, 0xff, sizeof(list));

    assert(!ahci_prepare_transfer(cmd, &list, 0, 1, 0, 0));
    assert(list.flags == (AHCI_CMD_ATAPI | 5));
    assert(!list.bytes && !cmd->prdt[0].base && !cmd->prdt[0].flags);
    assert(!cmd->prdt[0].baseu && !cmd->prdt[0].res);
    assert(!ahci_prepare_transfer(cmd, &list, 0, 1, buffer, 2048));
    assert(list.flags == ((1u << 16) | AHCI_CMD_ATAPI | 5));
    assert(cmd->prdt[0].base == 0x10000 && cmd->prdt[0].flags == 2047);
    assert(!ahci_prepare_transfer(cmd, &list, 1, 0, buffer, 65536));
    assert(list.flags == ((1u << 16) | AHCI_CMD_WRITE | 5));
    assert(cmd->prdt[0].flags == 65535);
    assert(!ahci_prepare_transfer(cmd, &list, 0, 0, buffer, 4u << 20));
    assert(cmd->prdt[0].flags == 0x3fffff);
    assert(ahci_prepare_transfer(cmd, &list, 0, 0, buffer, (4u << 20) + 2));
    assert(ahci_prepare_transfer(cmd, &list, 0, 0, buffer, 3));
    assert(ahci_prepare_transfer(cmd, &list, 0, 0, 0, 2048));
    assert(!ahci_prepare_transfer(cmd, &list, 0, 0, 0, 0));
    assert(list.flags == 5 && !cmd->prdt[0].flags);
    return 0;
}
"""
cc = shutil.which("cc") or shutil.which("clang")
if not cc:
    raise SystemExit("native C compiler required")
with tempfile.TemporaryDirectory(prefix="seabios-atapi-") as temp:
    test = Path(temp) / "test.c"
    binary = Path(temp) / "test"
    test.write_text(program, encoding="utf-8")
    subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

ata = (ROOT / "src/hw/ata.c").read_text(encoding="utf-8")
pio = ata[ata.index("static int\nata_pio_transfer("):]
assert pio.index("if (!count)") < pio.index("for (;;) {")
packet = ata[ata.index("int\nata_atapi_process_op("):]
assert "else if (status & ATA_CB_STAT_DRQ)" in packet
assert packet.index("unexpected data phase") < packet.index("fail:")
assert "outb(ATA_CB_DC_HD15, iobase2+ATA_CB_DC);" in packet
print("ATAPI descriptor and non-data command checks passed")
