# Legacy Windows Plug and Play BIOS and PCI routing

The firmware implements the BBS version check (function 60h) but does
not yet implement the mandatory system-device services (00h–02h)
defined in the Plug and Play BIOS Specification 1.0A, sections 4.4–4.5.
The BBS header is supplied directly to option ROMs and intentionally
not aligned to an OS-scannable boundary by default. The experimental
CONFIG_PNPBIOS_RUNTIME option restores the former advertisement for
development; it is not a complete PnP BIOS implementation.

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
