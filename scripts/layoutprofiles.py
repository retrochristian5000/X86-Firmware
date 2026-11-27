# SPDX-License-Identifier: GPL-3.0-or-later
"""Variant-specific policy for SeaBIOS's coordinated linker scripts."""

from dataclasses import dataclass


@dataclass(frozen=True)
class LayoutProfile:
    name: str
    min_biostable: int
    compact_init_text: bool = False


def select_profile(config, variant="auto", compact=False):
    """Keep firmware identity separate from size and placement policy."""
    size = config.get("CONFIG_ROM_SIZE", 0)
    if variant == "auto":
        if config.get("CONFIG_MULTIBOOT"):
            variant = "multiboot"
        elif config.get("CONFIG_CSM"):
            variant = "csm"
        elif config.get("CONFIG_COREBOOT"):
            variant = "coreboot"
        elif config.get("CONFIG_QEMU") and size in (128, 256):
            # The size selects the standard placement policy, not a machine
            # identity. Specialized profiles must still be requested explicitly.
            variant = "seabios-%dk" % size
        else:
            variant = "generic"
    expected = {"seabios-128k": 128, "seabios-256k": 256,
                "seabios-microvm": 128,
                "seabios-128k-external-ide": 128}
    if variant in expected:
        if size != expected[variant] or not config.get("CONFIG_QEMU"):
            raise ValueError("ROM layout variant does not match configuration: "
                             + variant)
    elif variant == "seabios-grub":
        if not config.get("CONFIG_MULTIBOOT") or not config.get("CONFIG_COREBOOT"):
            raise ValueError("GRUB layout requires the coreboot Multiboot entry")
    elif variant not in ("generic", "multiboot", "csm", "coreboot"):
        raise ValueError("Unknown ROM layout variant: " + variant)

    if variant == "seabios-128k-external-ide":
        if (config.get("CONFIG_ATA") or not config.get("CONFIG_OPTIONROMS")
                or not config.get("CONFIG_DRIVES")
                or not config.get("CONFIG_FLOPPY")):
            raise ValueError("External IDE layout requires ATA disabled, "
                             "option ROMs, disk services and floppy enabled")

    # Preserve the original effective reservation. Packing is opt-in and
    # restricted to movable initialization text in the two 128 KiB ROMs.
    if compact and variant not in ("seabios-128k", "seabios-microvm"):
        raise ValueError("Compact layout is supported only for 128 KiB QEMU ROMs")
    return LayoutProfile(variant, 2048, compact)


def compact_text_order(sections):
    """Stable descending alignment; never lower an input alignment."""
    return sorted(sections, key=lambda section: -section.align)
