Weitek 4167 firmware support
============================

SeaBIOS can advertise an emulated Weitek 4167 floating-point coprocessor to
legacy software through the extended INT 11h equipment interface used by
several 386/486 BIOS implementations.

Firmware configuration
----------------------

The platform may provide the fw_cfg/romfile integer:

```
etc/weitek4167
```

A non-zero value indicates that a Weitek 4167 is present.  On QEMU this file
should only be installed when the emulated 4167 device is actually mapped into
the guest physical address space.

INT 11h
-------

When `etc/weitek4167` is non-zero, INT 11h returns EAX bit 24 set in addition
to the normal equipment word in AX.

Bit 23 is intentionally left clear.  Historically that bit indicated that the
BIOS had arranged page tables so the Weitek coprocessor address space could be
used from real mode.  SeaBIOS does not currently provide such a mapping, so it
must not advertise that capability.

This distinction is important for DOS memory managers and other legacy
software: coprocessor presence and real-mode addressability are separate
capabilities.
