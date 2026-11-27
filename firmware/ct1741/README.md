# CT1741 DSP firmware prototype

An original, independently compiled MCS-51 program for an experimental
Sound Blaster 16 CT1741 firmware path. This is not Creative's ROM and
does not contain proprietary firmware bytes. It is not a complete
CT1741 replacement.

## Build

Install SDCC with its MCS-51 backend and makebin. From the repository root:

    make -C firmware/ct1741 check
    make -C firmware/ct1741

The host tests need only a C99 compiler. The MCU build produces
`firmware/ct1741/build/ct1741.bin`, an exactly 8192-byte image, plus
SDCC's Intel HEX and linker reports. The linker is constrained to 8192
bytes of code and 256 bytes of internal RAM. Inspect the linker memory
report for stack headroom when expanding the program. No generated
binary is checked in, and the root SeaBIOS build does not depend on
this directory or on SDCC. The previously added opt-in x86 POST
diagnostic is separate and remains disabled by default.

## Processor and internal interface

Intel 8052: reset at 0000h, 8 KiB program ROM and 256 bytes internal RAM.
The following internal interface is an experimental starting point
based on the independently implemented MAME CT1741 model, not a verified
physical CT1741 pinout:

| MCU address | Direction | Meaning |
| --- | --- | --- |
| SFR P1 (90h), bit 0 | input | Host output latch full |
| SFR P1 (90h), bit 1 | input | Host input latch ready |
| XDATA 0000h | read | Consume host command/parameter byte |
| XDATA 0000h | write | Publish response byte |

The host must clear the output-full flag when consuming a response and
clear input-ready on MCU read; reset must clear both latches and reset
the MCU. The firmware waits rather than overwriting unread responses.
A real implementation needs independently scheduled MCU execution;
the host must not synchronously spin waiting for the MCU. The addresses
above are internal to the DSP, not PC ISA I/O ports.

## Implemented command subset

Reset returns AAh; E0h returns the complement of its parameter; E1h
reports the provisional compatibility version 4.05 in major/minor order;
E4h stores a test byte and E8h reads it back. The version is not a claim
of equivalence to the original Creative 4.05 firmware. The host test
checks response ordering, all 256 identification values, parameter
framing, reset and backpressure. Unsupported commands are ignored
without parameter framing; do not use this image as a general-purpose
Sound Blaster driver target.

## Integration boundary

QEMU must supply an 8052 CPU core, program ROM mapping, host latches,
reset wiring and the remaining DMA, IRQ, DAC/ADC and clock interfaces.
Do not point an x86 option-ROM loader at this image. No existing QEMU
device loads or executes this image yet. Do not enable it by default
or replace the current SB16 implementation before PCM and driver tests
pass. No physical card flashing is supported.

References:
- Intel MCS-51 family hardware: https://www.keil.com/dd/chip/3425.htm
- Creative Sound Blaster Series Hardware Programming Guide, chapters 2 and 6:
  https://pdos.csail.mit.edu/6.828/2018/readings/hardware/SoundBlaster.pdf
- Independent CT1741 model, MAME 0.289:
  https://sources.debian.org/src/mame/0.289%2Bdfsg1-1/src/devices/sound/ct1741.cpp
- SDCC: https://sdcc.sourceforge.net/
