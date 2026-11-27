# CT1741 DSP firmware prototype

An original, independently compiled MCS-51 program for an experimental
Sound Blaster 16 CT1741 firmware path. This is not Creative's ROM and
does not contain proprietary firmware bytes. It is not a complete
CT1741 replacement.

## Build

Install SDCC with its MCS-51 backend and makebin. From the repository root:

    make -C firmware/ct1741 check
    make -C firmware/ct1741

For an out-of-tree provider build, pass `BUILD_DIR=/absolute/output/path`.
The caller owns that directory; `clean` only removes the default local
`build` directory. The provider may use a fresh output directory to avoid
reusing objects compiled by a different SDCC installation.

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
framing, reset and backpressure. PCM commands now produce hardware actions through the private v1 mailbox below. Known unsupported parameter frames (including ADC) are consumed without performing those operations. Unknown commands remain unsupported.

## Integration boundary

QEMU must supply an 8052 CPU core, program ROM mapping, host latches,
reset wiring and the remaining DMA, IRQ, DAC/ADC and clock interfaces.
Do not point an x86 option-ROM loader at this image. QEMU's experimental CT1741 firmware transport loads and executes this image. Do not enable it by default
or replace the current SB16 implementation before PCM and driver tests
pass. No physical card flashing is supported.

References:
- Intel MCS-51 family hardware: https://www.keil.com/dd/chip/3425.htm
- Creative Sound Blaster Series Hardware Programming Guide, chapters 2 and 6:
  https://pdos.csail.mit.edu/6.828/2018/readings/hardware/SoundBlaster.pdf
- Independent CT1741 model, MAME 0.289:
  https://sources.debian.org/src/mame/0.289%2Bdfsg1-1/src/devices/sound/ct1741.cpp
- SDCC: https://sdcc.sourceforge.net/

## Experimental PCM hardware interface v1

The firmware owns host command framing. XDATA 0101h–0104h holds four
argument bytes; writing 0100h commits one action. This is an invented
internal emulation ABI, not a Creative register map. The card supplies
DMA, sample conversion, speaker control and IRQ acknowledgement. Actions:
1 rate (big-endian), 2 time constant, 3 legacy block length (little-endian
minus one), 4 PCM start (command, mode, length low, length high),
5 legacy start (command, length low, length high), 6 DMA control
(command), 7 speaker (0/1), 8 test IRQ (0=8-bit, 1=16-bit), 9 DMA identification byte.
Playback supports B0/B2/B4/B6 and C0/C2/C4/C6, legacy 14h/1Ch/90h/91h,
40h/41h/48h and pause/resume/exit commands. Capture, direct DAC, ADPCM,
cycle-accurate timers remain unimplemented.

### DMA identification (E2h)

The MCU computes a cumulative eight-bit response, initialized to AAh,
using XOR masks 96h, A5h, 69h, 5Ah in repeating order. Reset restarts
this sequence. The response is committed as action 9 with argument 0;
no host data-port reply or audio IRQ is generated. The board transfers
one byte on the configured low ISA DMA channel, subject to its DMA
controller. This is an extension of the private v1 mailbox, not a
physical CT1741 register definition. Compatibility reference (undocumented
command): https://dosbox-x.com/doxygen/html/sblaster_8cpp_source.html .
Physical CT1741 verification is still needed.
