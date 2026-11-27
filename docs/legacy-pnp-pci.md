# Legacy Windows Plug and Play BIOS and PCI routing

The firmware supplies the BBS header and version check (function 60h)
directly to option ROMs, independently of OS discovery. The experimental
CONFIG_PNPBIOS_RUNTIME option is disabled by default. On a QEMU PIIX3
or PIIX4 platform with the router at 00:01.0, it now publishes a separate
aligned installation structure after PCI initialization.

Functions 00h and 01h preserve the PCI root (handle 0, PNP0A03) and
ISA bus (handle 1, PNP0A00). With the QEMU boot-time presence file
`etc/isa-pnp-core-v1` (one byte, bits 0–3), these fixed nodes are added:

| Handle | PnP ID | Allocated resources |
|---|---|---|
| 2 | PNP0000 | 20–21h, A0–A1h, 4D0–4D1h; IRQ2 cascade |
| 3 | PNP0200 | 00–0Fh, 81–83h, 87h, 89–8Bh, 8Fh, C0–DFh; DMA4 cascade |
| 4 | PNP0100 | 40–43h; IRQ0 |
| 5 | PNP0B00 | 70–71h; IRQ8 |

The BIOS reports the actual count and maximum node size, skips absent
handles and ends enumeration with FFh. Invalid or missing presence files
fall back to the two original bus nodes. Current and boot configurations
are identical; function 02h rejects modifications to fixed nodes.
Each has three terminated resource blocks. No runtime hardware writes
or expansion-card identification are performed. DMA1, DMA5 and other
peripheral channels are not claimed by the DMA controller node.

Function 40h returns the six-byte ISA configuration structure (revision 1,
zero assigned card select numbers, invalid zero read-data port, zero
reserved word). A zero count does not mean that no legacy ISA devices
are present. No ISA PnP isolation or configuration is performed.

This remains a partial inventory, not a compliant PnP BIOS. Keyboard,
other motherboard devices, ISA PnP card isolation and ESCD services
remain to be implemented. Do not enable runtime
discovery for normal guests or assume this resolves NDIS calibration
failures.

PCI BIOS INT 1Ah B1xx and the BIOS32 directory are independent.
The built-in PIIX routing table now identifies a discovered PIIX3
(8086:7000) or PIIX4 (8086:7110) router at 00:01.0. It retains
the existing six root-bus slot mappings. Q35 and unknown routers
do not receive this PIIX-specific fallback table. Imported tables
must use version 1.0 and complete 16-byte routing entries.

PCI BIOS 2.1 B10Fh programs PIIX3/PIIX4 root-bus interrupt routes
using the published $PIR table, verified router identity, and the
intersection of the table's IRQ bitmap with the chipset's legal IRQs.
It sets the target ELCR bit before enabling the requested PIRQ route
and verifies both writes. On readback failure it attempts to restore
the original state. Other route registers and ELCR bits are preserved.
The caller is responsible for resource conflicts and updating Interrupt
Line registers of all affected PCI functions; the routing is not
persisted across boots. Secondary-bus routing and other routers are
not implemented by this service. Unsupported platforms return AH=81h;
unavailable routes return AH=88h, both with carry set.

B106h explicitly returns AH=81h with carry set: the QEMU PCI host
does not provide special-cycle broadcasts. B101h continues to report
mechanism 1 without special-cycle capability bits. No ordinary config
write is substituted for a broadcast. The reported BIOS version stays
2.10. See PCI BIOS Specification revision 2.1 sections 4.2.1 and 4.2.3
and Appendix B.

The host regression is `tests/pci-irq-route-test.c`; build with
`cc -std=gnu11 -Wall -Wextra -Werror tests/pci-irq-route-test.c -o pci-irq-route-test`.
It checks the route helper with mocked I/O; real-mode and protected-mode
BIOS entry-point testing is still required.

During PCI initialization, BAR sizing and assignment temporarily clear
the I/O and memory decode bits using a 16-bit Command register access.
Both halves of a 64-bit BAR are probed before either mask is read.
The original BAR values are restored before the original Command value;
an already-disabled device is left disabled. This preserves unrelated
Command bits and avoids writing the adjacent write-one-to-clear Status
register. The same decode guard covers expansion ROM BAR sizing.
The allocator's existing resource limits and VGA selection policy are
unchanged; bridge-window programming and multi-adapter routing require
separate validation.

The host regression is `tests/pci-bar-probe-test.c`; build with
`cc -std=gnu11 -Wall -Wextra -Werror -Isrc tests/pci-bar-probe-test.c -o pci-bar-probe-test`.
It uses mocked configuration accesses, not live hardware.

Legacy DMA POST initialization resets DMA2 before DMA1 to close the
cascade while configuring both controllers. All seven peripheral
channels receive the demand/verify, incrementing, non-autoinitializing
mode (the PIIX4 reset defaults); system channel 4 receives cascade
mode and is the only channel unmasked. Address, count and page
registers are not initialized by POST because peripheral drivers must
program them before unmasking their channels. The floppy DMA transfer
and completion paths are unchanged.

See Intel 82371AB PIIX4 datasheet sections 4.2.1.2 and 4.2.1.7-11.
The host test `tests/dma-init-test.c` checks the exact port write
sequence twice, without performing live DMA. Build with
`cc -std=gnu11 -Wall -Wextra -Werror tests/dma-init-test.c -o dma-init-test`.
This does not change the PnP BIOS inventory or provide PC/PCI or
distributed DMA services.

Default IRQ7 and IRQ15 vectors now have dedicated handlers. With normal
cascaded 8259A operation (non-auto EOI, fully nested mode), the handler
checks ISR bit 7 before acknowledging the corresponding controller.
A spurious IRQ7 sends no EOI; a spurious IRQ15 sends only a specific
master IRQ2 EOI. Real IRQ7/15 use specific IRQ7 EOIs on the appropriate
PIC, and the slave path then acknowledges master IRQ2. Other ISR bits
are not cleared by these handlers. Guest-installed handlers and existing
device-specific BIOS interrupt handlers remain unchanged.

See Intel 82371AB PIIX4 datasheet section 4.2.2 and the 8259A default
IR7 behavior, https://htmlapp.alldatasheet.com/html-pdf/66088/INTEL/82371AB/3139/24/82371AB.html.
The host regression exercises all 256 ISR bitmaps for each vector:
`cc -std=gnu11 -Wall -Wextra -Werror tests/pic-spurious-test.c -o pic-spurious-test`.
It checks EOI port writes, not real interrupt delivery or ROM size.

Rebuild the BIOS, cold boot the same machine, and verify the Windows
Device Manager status and PCI IRQ steering. An existing Windows device
record can persist even when firmware advertisement changes; do not
remove guest devices or drivers automatically. No guest timing or CPU
settings are changed by this firmware work.

Reference: Plug and Play BIOS Specification 1.0A, May 5, 1994.
https://www.osdever.net/documents/PNPBIOSSpecification-v1.0a.pdf

The fixed node host regression is `tests/pnp-fixed-nodes-test.c`.
The real-mode regression `scripts/test-isa-pnp.py` requires the matching
QEMU build that publishes the presence file and a runtime-enabled ROM.
