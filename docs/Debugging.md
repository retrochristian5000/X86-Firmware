This page describes the process of obtaining diagnostic information
from SeaBIOS and for reporting problems.

Diagnostic information
======================

SeaBIOS has the ability to output diagnostic messages. This is
implemented in the code via calls to the "dprintf()" C function.

On QEMU these messages are written to a special debug port. One can
view these messages by adding '-chardev stdio,id=seabios -device
isa-debugcon,iobase=0x402,chardev=seabios' to the QEMU command line.
Once this is done, one should see status messages on the console.

On coreboot these messages are generally written to the "cbmem"
console (CONFIG_DEBUG_COREBOOT). If SeaBIOS launches a Linux operating
system, one can obtain the cbmem tool from the coreboot repository and
run "cbmem -c" to view the SeaBIOS diagnostic messages.

Additionally, if a serial port is available, one may compile SeaBIOS
to send the diagnostic messages to the serial port. See the SeaBIOS
CONFIG_DEBUG_SERIAL option.

Trouble reporting
=================

If you are experiencing problems with SeaBIOS, it's useful to increase
the debugging level. This is done by running "make menuconfig" and
setting CONFIG_DEBUG_LEVEL to a higher value. A debug level of 8 will
show a lot of diagnostic information without flooding the serial port
(levels above 8 will frequently cause too much data).

To report an issue, please collect the serial boot log with SeaBIOS
set to a debug level of 8 and forward the full log along with a
description of the problem to the SeaBIOS [mailing
list](Mailinglist.md).

Opt-in QEMU guest-memory footprint report
=========================================

For the WHP X86-Firmware fork, enable a one-time firmware allocation report
at the **end** of POST, after unused high-memory pages have been returned:

```
qemu-system-i386 -m 64M -bios out/bios.bin \
    -fw_cfg name=opt/org.seabios/whp-memory-report,string=1 \
    -chardev file,id=seabios,path=seabios-memory.log \
    -device isa-debugcon,iobase=0x402,chardev=seabios
```

The `WHP SeaBIOS memory:` messages identify the conventional memory
limit (KiB), EBDA size (KiB), tracked permanent allocations in the
low, F-segment and high zones (bytes), and the whole pages returned
from the high zone (bytes). They also identify whether the low zone
uses upper/option-ROM space or conventional RAM. The option's **presence**
enables the diagnostic; no additional SeaBIOS allocations or E820
reservations are made for reporting.

This is an allocator footprint, **not** the total QEMU process RSS and
not the total unavailable guest RAM. Static BIOS/F-segment contents,
PCI MMIO/option ROM address holes, ACPI/NVS reservations, and temporary
allocations are not all represented by the dynamic-zone totals. The
ordinary SeaBIOS E820 debug listing at level 1 provides the final
guest memory map. Compare identical machine types, `-m` values,
attached devices and boot media before attributing a change to
firmware. `CONFIG_MALLOC_UPPERMEMORY` must not be disabled just to
reduce ROM-space usage: doing so can consume conventional memory
needed by DOS applications. A report by itself does not modify
the firmware memory policy.

Extended QEMU POST self-test
===========================

SeaBIOS already performs a real POST: legacy interrupt and data-area
initialization, DMA/PIC/timer setup, virtual device discovery, VGA and option
ROM execution, and boot preparation. A fast virtual POST normally trusts the
RAM map provided by QEMU. For an **active** memory check, the WHP fork offers
an explicitly requested extended POST:

```
qemu-system-i386 -m 64M -bios out/bios.bin \
    -fw_cfg name=opt/org.seabios/whp-extended-post,string=1 \
    -chardev file,id=seabios,path=seabios-post.log \
    -device isa-debugcon,iobase=0x402,chardev=seabios
```

With this option, SeaBIOS checks that the E820 map is sorted, has no
overlaps/overflow/zero-size entries and contains RAM; checks the BIOS Data
Area conventional-memory limit and the Extended BIOS Data Area boundary
and reservation; then performs two independent read/write pattern passes,
plus walking-one and walking-zero bit tests, on a freshly allocated
**4 KiB temporary RAM buffer**. It frees the buffer before boot. On a
failure the firmware prints `WHP POST FAIL` and halts before the boot
interrupt; on success it prints `WHP extended POST: ... PASS`. This
option is QEMU-specific and disabled during normal boots.

These tests *exercise real RAM reads and writes* inside the emulator but
do **not** verify every byte of guest RAM, a physical memory module,
CPU silicon, or every emulated device. The normal initialization of
PCI, storage, keyboard, timer and display remains controlled by the
hardware configuration; a missing optional device is not incorrectly
classified as a POST failure. Use the existing memory-footprint option
(`whp-memory-report`) independently when investigating RAM consumption.
Do not replace these active tests with a cosmetic full-screen POST
counting animation.

Timing debug messages
=====================

The SeaBIOS repository has a tool (**scripts/readserial.py**) that can
timestamp each diagnostic message produced. The timestamps can provide
some additional information on how long internal processes take. It
also provides a simple profiling mechanism.

The tool can be used on coreboot builds that have diagnostic messages
sent to a serial port. Make sure SeaBIOS is configured with
CONFIG_DEBUG_SERIAL and run the following on the host receiving serial
output:

`/path/to/seabios/scripts/readserial.py /dev/ttyS0 115200`

Update the above command with the appropriate serial device and baud
rate.

The tool can also timestamp the messages from the QEMU debug port. To
use with QEMU run the following:

```
mkfifo qemudebugpipe
qemu -chardev pipe,path=qemudebugpipe,id=seabios -device isa-debugcon,iobase=0x402,chardev=seabios ...
```

And then in another session:

`/path/to/seabios/scripts/readserial.py -nf qemudebugpipe`

The mkfifo command only needs to be run once to create the pipe file.

When readserial.py is running, it shows a timestamp with millisecond
precision of the amount of time since the start of the log. If one
presses the "enter" key in the readserial.py session it will add a
blank line to the screen and also reset the time back to zero. The
readserial.py program also keeps a log of all output in files that
look like "seriallog-YYYYMMDD_HHMMSS.log".

Debugging with gdb on QEMU
==========================

One can use gdb with QEMU to debug system images. To do this, add '-s
-S' to the QEMU command line. For example:

`qemu -bios out/bios.bin -fda myfdimage.img -s -S`

Then, in another session, run gdb with either out/rom16.o (to debug
bios 16bit code) or out/rom.o (to debug bios 32bit code). For example:

`gdb out/rom16.o`

Once in gdb, use the command `target remote localhost:1234` to have
gdb connect to QEMU. See the QEMU documentation for more information
on using gdb and QEMU in this mode.

When debugging 16bit code it is necessary to load the 16bit symbols
twice in order for gdb to properly handle break points.  To do this,
run the following command `objcopy --adjust-vma 0xf0000 out/rom16.o
rom16offset.o` and then run the following in gdb:

```
set architecture i8086
add-symbol-file rom16offset.o 0
```

To debug a VGA BIOS image, run `gdb out/vgarom.o`, create a
vgaromoffset.o file with offset 0xc0000, add use the gdb
command `add-symbol-file out/vgaromoffset.o 0` to load the 16bit VGA
BIOS symbols twice.

If debugging the 32bit SeaBIOS initialization code with gdb, note that
SeaBIOS does self relocation by default. This relocation will alter
the location of initialization code symbols. Disable
CONFIG_RELOCATE_INIT to prevent SeaBIOS from doing this.
