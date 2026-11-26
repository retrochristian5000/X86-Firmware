# Legacy Windows Plug and Play BIOS and PCI routing

The firmware supplies the BBS header and version check (function 60h)
directly to option ROMs, independently of OS discovery. The experimental
CONFIG_PNPBIOS_RUNTIME option is disabled by default. On a QEMU PIIX3
or PIIX4 platform with the router at 00:01.0, it now publishes a separate
aligned installation structure after PCI initialization.

Functions 00h and 01h report two fixed nodes: PNP0A03 PCI root bus
(handle 0, configuration I/O CF8h–CFFh) and PNP0A00 ISA bus (handle 1,
no individually claimed resources). Each has three terminated resource
blocks and identical current and boot configuration. Enumeration returns
FFh after the ISA node. Function 02h rejects attempts to configure either
bus; no hardware registers are written by these runtime calls.

Function 40h returns the six-byte ISA configuration structure (revision 1,
zero assigned card select numbers, invalid zero read-data port, zero
reserved word). A zero count does not mean that no legacy ISA devices
are present. No ISA PnP isolation or configuration is performed.

This is only a partial inventory, not a compliant PnP BIOS. PIC, PIT,
RTC, DMA, keyboard and other motherboard devices, ISA PnP card isolation
and ESCD services remain to be implemented. Do not enable runtime
discovery for normal guests or assume this resolves NDIS calibration
failures.

PCI BIOS INT 1Ah B1xx and the BIOS32 directory are independent.
The built-in PIIX routing table now identifies a discovered PIIX3
(8086:7000) or PIIX4 (8086:7110) router at 00:01.0. It retains
the existing six root-bus slot mappings. Q35 and unknown routers
do not receive this PIIX-specific fallback table. Imported tables
must use version 1.0 and complete 16-byte routing entries.

Rebuild the BIOS, cold boot the same machine, and verify the Windows
Device Manager status and PCI IRQ steering. An existing Windows device
record can persist even when firmware advertisement changes; do not
remove guest devices or drivers automatically. No guest timing or CPU
settings are changed by this firmware work.

Reference: Plug and Play BIOS Specification 1.0A, May 5, 1994.
https://www.osdever.net/documents/PNPBIOSSpecification-v1.0a.pdf
