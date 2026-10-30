SeaBIOS can read several configuration items at runtime. On coreboot
the configuration comes from files located in CBFS. When SeaBIOS runs
natively on QEMU the files are passed from QEMU via the fw_cfg
interface.

This page documents the user visible configuration and control
features that SeaBIOS supports.

LZMA compression
================

On coreboot, when scanning files in CBFS, any filename that ends with
a ".lzma" suffix will be treated as a raw file that is compressed with
the lzma compression algorithm. This works for option ROMs,
configuration files, floppy images, etc. . (This feature should not be
used with embedded payloads - to compress payloads, use the standard
section based compression algorithm that is built into the payload
specification.)

For example, the file **pci1106,3344.rom.lzma** would be treated the
same as **pci1106,3344.rom**, but will be automatically uncompressed
when accessed.

A file is typically compressed with the lzma compression command line
tool. For example:

`lzma -zc /path/to/somefile.bin > somefile.bin.lzma`

However, some recent versions of lzma no longer supply an uncompressed
file size in the lzma header. (They instead populate the field with
zero.) Unfortunately, SeaBIOS requires the uncompressed file size, so
it may be necessary to use a different version of the lzma tool.

File aliases
============

It is possible to create the equivalent of "symbolic links" so that
one file's content appears under another name. To do this, create a
**links** file with one line per link and each line having the format
of "linkname" and "destname" separated by a space character. For
example, the **links** file may look like:

```
pci1234,1000.rom somerom.rom
pci1234,1001.rom somerom.rom
pci1234,1002.rom somerom.rom
```

The above example would cause SeaBIOS to treat "pci1234,1000.rom" or
"pci1234,1001.rom" as files with the same content as the file
"somerom.rom".

Option ROMs
===========

SeaBIOS will scan all of the PCI devices in the target machine for
option ROMs on PCI devices. It recognizes option ROMs in files that
have the form **pciVVVV,DDDD.rom**. The VVVV,DDDD should correspond to
the PCI vendor and device id of a device in the machine. If a given
file is found then SeaBIOS will deploy the file instead of attempting
to extract an option ROM from the device. In addition to supplying
option ROMs for on-board devices that do not store their own ROMs,
this mechanism may be used to prevent a ROM on a specific device from
running.

SeaBIOS always deploys the VGA rom associated with the active VGA
device before any other ROMs.

In addition, SeaBIOS will also run any file in the directory
**vgaroms/** as a VGA option ROM not specific to a device and files in
**genroms/** as a generic option ROM not specific to a device. The
ROMS in **vgaroms/** are run immediately after running the option ROM
associated with the primary VGA device (if any were found), and the
**genroms/** ROMs are run after all other PCI ROMs are run.

Bootsplash images
=================

SeaBIOS can show a custom [JPEG](http://en.wikipedia.org/wiki/JPEG)
image or [BMP](http://en.wikipedia.org/wiki/BMP_file_format) image
during bootup. To enable this, add the JPEG file to flash with the
name **bootsplash.jpg** or BMP file as **bootsplash.bmp**.

The size of the image determines the video mode to use for showing the
image. Make sure the dimensions of the image exactly correspond to an
available video mode (eg, 640x480, or 1024x768), otherwise it will not
be displayed.

SeaBIOS will show the image during the wait for the boot menu (if the
boot menu has been disabled, users will not see the image). The image
should probably have "Press ESC for boot menu" embedded in it so users
know they can enter the normal SeaBIOS boot menu. By default, the boot
menu prompt (and thus graphical image) is shown for 2.5 seconds. This
can be customized via a [configuration
parameter](#Other_Configuration_items).

The JPEG viewer in SeaBIOS uses a simplified decoding algorithm. It
supports most common JPEGs, but does not support all possible formats.
Please see the [trouble reporting section](Debugging.md) if a valid
image isn't displayed properly.

Payloads
========

On coreboot, SeaBIOS will treat all files found in the **img/**
directory as a coreboot payload. Each payload file will be available
for boot, and one can select from the available payloads in the
bootmenu. SeaBIOS supports both uncompressed and lzma compressed
payloads.

Floppy images
=============

It is possible to embed an image of a floppy into a file. SeaBIOS can
then boot from and redirect floppy BIOS calls to the image. This is
mainly useful for legacy software (such as DOS utilities). To use this
feature, place a floppy image into the directory **floppyimg/**.

Using LZMA file compression with the [.lzma file
suffix](#LZMA_compression) is a useful way to reduce the file
size. Several floppy formats are available: 360K, 1.2MB, 720K, 1.44MB,
2.88MB, 160K, 180K, 320K.

The floppy image will appear as writable to the system, however all
writes are discarded on reboot.

When using this system, SeaBIOS reserves high-memory to store the
floppy. The reserved memory is then no longer available for OS use, so
this feature should only be used when needed.

Configuring boot order
======================

The **bootorder** file may be used to configure the boot up order. The
file should be ASCII text and contain one line per boot method. The
description of each boot method follows an [Open
Firmware](https://secure.wikimedia.org/wikipedia/en/wiki/Open_firmware)
device path format. SeaBIOS will attempt to boot from each item in the
file - first line of the file first.

The easiest way to find the available boot methods is to look for
"Searching bootorder for" in the SeaBIOS debug output. For example,
one may see lines similar to:

```
Searching bootorder for: /pci@i0cf8/*@f/drive@1/disk@0
Searching bootorder for: /pci@i0cf8/*@f,1/drive@2/disk@1
Searching bootorder for: /pci@i0cf8/usb@10,4/*@2
```

The above represents the patterns SeaBIOS will search for in the
bootorder file. However, it's safe to just copy and paste the pattern
into bootorder. For example, the file:

```
/pci@i0cf8/usb@10,4/*@2
/pci@i0cf8/*@f/drive@1/disk@0
```

will instruct SeaBIOS to attempt to boot from the given USB drive
first and then attempt the given ATA harddrive second.

SeaBIOS also supports a special "HALT" directive. If a line that
contains "HALT" is found in the bootorder file then SeaBIOS will (by
default) only attempt to boot from devices explicitly listed above
HALT in the file.

Other Configuration items
=========================

There are several additional configuration options available in the
**etc/** directory.

| Filename            | Description
|---------------------|---------------------------------------------------
| show-boot-menu      | Controls the display of the boot menu. Valid values are 0: Disable the boot menu, 1: Display boot menu unconditionally, 2: Skip boot menu if only one device is present. The default is 1.
| boot-menu-message   | Customize the text boot menu message. When omitted, SeaBIOS names the selected Esc or F1-F12 key automatically (Esc is the default). For other scan codes, it displays a generic prompt. This field overrides the generated prompt.
| boot-menu-key       | Controls which key activates the **boot menu**, not BIOS Setup. The value is the binary BIOS scan code (e.g. 0x43 for F9, 0x86 for F12, 0x01 for Esc), not the literal text "0x43". See the F9 example below.
| boot-menu-wait      | Amount of time (in milliseconds) to wait at the boot menu prompt before selecting the default boot. Set to a negative number such as -1 to force the display of the boot menu.
| boot-fail-wait      | If no boot devices are found SeaBIOS will reboot after 60 seconds. Set this to the amount of time (in milliseconds) to customize the reboot delay or set to -1 to disable rebooting when no boot devices are found
| extra-pci-roots     | If the target machine has multiple independent root buses set this to a positive value. The SeaBIOS PCI probe will then search for the given number of extra root buses.
| ps2-keyboard-spinup | Some laptops that emulate PS2 keyboards don't respond to keyboard commands immediately after powering on. One may specify the amount of time (in milliseconds) here to allow as additional time for the keyboard to become responsive. When this field is set, SeaBIOS will repeatedly attempt to detect the keyboard until the keyboard is found or the specified timeout is reached.
| optionroms-checksum | Option ROMs are required to have correct checksums. However, some option ROMs in the wild don't correctly follow the specifications and have bad checksums. Set this to a zero value to allow SeaBIOS to execute them anyways.
| pci-optionrom-exec  | Controls option ROM execution for roms found on PCI devices (as opposed to roms found in CBFS/fw_cfg).  Valid values are 0: Execute no ROMs, 1: Execute only VGA ROMs, 2: Execute all ROMs. The default is 2 (execute all ROMs).
| s3-resume-vga-init  | Set this to a non-zero value to instruct SeaBIOS to run the vga rom on an S3 resume.
| screen-and-debug    | Set this to a zero value to instruct SeaBIOS to not write characters it sends to the screen to the debug ports. This can be useful when using sgabios.
| advertise-serial-debug-port | If using a serial debug port, one can set this file to a zero value to prevent SeaBIOS from listing that serial port as available for operating system use. This can be useful when running old DOS programs that are known to reset the baud rate of all advertised serial ports.
| sercon-port         | Set this to the IO address of a serial port to enable SeaBIOS' VGA adapter emulation on the given serial port.
| floppy0             | Set this to the type of the first floppy drive in the system (only type 4 for 3.5 inch drives is supported).
| floppy1             | The type of the second floppy drive in the system. See the description of **floppy0** for more info.
| threads             | By default, SeaBIOS will parallelize hardware initialization during bootup to reduce boot time. Multiple hardware devices can be initialized in parallel between vga initialization and option rom initialization. One can set this file to a value of zero to force hardware initialization to run serially. Alternatively, one can set this file to 2 to enable early hardware initialization that runs in parallel with vga, option rom initialization, and the boot menu.
| sdcard*             | One may create one or more files with an "sdcard" prefix (eg, "etc/sdcard0") with the physical memory address of an SDHCI controller (one memory address per file).  This may be useful for SDHCI controllers that do not appear as PCI devices, but are mapped to a consistent memory address. If this option is used then SeaBIOS will not scan for PCI SHDCI controllers.
| usb-time-sigatt     | The USB2 specification requires devices to signal that they are attached within 100ms of the USB port being powered on. Some USB devices are known to require more time. Prior to receiving an attachment signal there is no way to know if a USB port is empty or if it has a device attached. One may specify an amount of time here (in milliseconds, default 100) to wait for a USB device attachment signal. Increasing this value will also increase the overall machine bootup time.

Experimental AMIBIOS Hi-Flex CMOS overlay
=========================================

The WHP SeaBIOS fork has an *opt-in*, partial 1993 AMIBIOS Hi-Flex CMOS
compatibility overlay. With QEMU, enable it using:

```
-fw_cfg name=opt/org.seabios/ami-hiflex-cmos,string=1
```

The overlay initializes the standard and extended additive CMOS checksums
(2Eh/2Fh and 3Eh/3Fh), reserves the unknown chipset byte 36h as zero,
and selects the documented setup color at 37h. It is disabled by default.

**Important:** The Hi-Flex AMIBIOS 34h-35h ROM-shadow and numeric-processor
settings conflict with QEMU's existing 34h-35h extended-RAM-size bytes.
SeaBIOS must preserve QEMU's RAM-size data even when this overlay is enabled.
Bytes 38h-3Dh are likewise owned by existing QEMU boot-order and disk-translation
settings and are never cleared by this overlay. The extended checksum therefore
covers the *preserved QEMU values*, not a fully authentic AMIBIOS configuration.

This is not AMIBIOS ROM emulation. It does not provide AMI password checking,
ROM shadow control, vendor-specific INT services, POST checkpoints, or an
AMIBIOS setup UI. Do not use this mode as a substitute for a machine-specific
AMI BIOS/CMOS implementation.

Laptop APM 1.2 POST power snapshot (experimental)
==================================================
The existing INT 15h/AX=530Ah desktop default reports AC power and no battery.
A future laptop model or compatibility test can supply a *static POST snapshot*
over `-fw_cfg name=opt/org.seabios/whp-laptop-apm,file=...`. The record is
exactly eight binary bytes (not ASCII):
version=1; AC line BH (0,1,2,FFh); battery BL (0 high,1 low,2 critical,
3 charging,FFh unknown); battery flags CH (bits 0-3 status, bit 7 no
system battery, or FFh unknown); percent CL (0-100 or FFh); installed
battery count (0 or 1); and remaining time DX (little-endian; high bit
selects minutes, FFFFh unknown).

A deterministic **test fixture** for AC online, charging, 67%, one battery,
300 minutes remaining can be created by a shell that supports octal escapes:

```sh
printf '\001\001\003\010\103\001\054\201' > laptop-apm.bin
qemu-system-i386 -machine pc \
  -fw_cfg name=opt/org.seabios/whp-laptop-apm,file=laptop-apm.bin
```

An absent battery requires status FFh, flags 80h, percentage FFh, time FFFFh,
and count zero. Invalid sizes, versions, reserved flags, mismatched status
bits, contradictory absence and unsupported multiple-battery counts are
rejected; desktop defaults remain unchanged. AX=530Ah with a valid record
supports BX=0001h (system) or BX=8001h (battery 1 if present), rejecting
other device IDs with AH=09h. AX=5310h/BX=0000h reports the same number
of battery units in BL, but **no suspend/standby wake capabilities** in CX.

This record is sampled only once at POST. It is **not live** telemetry:
charge or AC status cannot change without another POST. No laptop, charger,
battery, lid, PCMCIA controller or suspend mechanism is invented here.
This feature does not change the separate Dell-style F2/F12 or AMI overlays.

PS/2 extended serial INT 14h
============================

SeaBIOS implements PS/2-compatible INT 14h/AH=05h extended communications
port control for an already-detected COM port (DX = 0..3). AL=00h reads the
UART modem-control register (MCR); AL=01h writes BL bits 0..4 to the MCR.
Both operations return the MCR in BL, line status in AH, and modem status
in AL. Unsupported subfunctions and absent COM ports have no hardware
side effects. This service is also documented for 1993-era AMIBIOS, but
it is **not** an AMI-exclusive vendor call, and it does not change the
firmware's identity or require enabling the experimental AMI CMOS overlay.

INT 14h/AH=04h extended port initialization and AMI-specific CPU-speed,
cache-controller, and EISA functions require their own compatibility
verification; this serial extension does not claim to implement them.

Optional Dell-style POST controls for WHP Dell machines
=======================================================

WHP's QEMU Dell System 425E, System 433E and PowerLine Workstation 433DE
machine types now supply a read-only `opt/org.seabios/whp-dell-model` fw_cfg
integer (respectively 1, 2 and 3), identifying the actual emulated model.
The model ID alone **does not** change the BIOS UI. To opt in to the
*modern Dell-inspired* F2/F12 workflow, boot one of those machines with:

```sh
qemu-system-i386 -machine dell-system-433de \
  -fw_cfg name=opt/org.seabios/whp-dell-post-ui,string=1
```

When the matching model ID and the opt-in Boolean are both present:

* **F12** opens SeaBIOS's existing **one-time** boot-device selection menu.
  This only changes the current startup selection, not persistent CMOS order.
* **F2** opens a read-only **System Information** display showing the emulated
  model/EISA ID, actual SeaBIOS build, reported low RAM and the Weitek
  coprocessor presence flag. ESC/Enter continues the normal boot; F12 enters
  the boot-device menu when F12 remains its configured activation key.
* The existing `etc/boot-menu-key` override takes precedence. Thus a
  caller who explicitly requests Esc, F9 or F2 as the boot-menu key keeps it.
  `etc/show-boot-menu=0` still disables this whole interaction.
* Other QEMU machines and normal Dell machine boot **remain unchanged**.
  `opt/org.seabios/whp-dell-post-ui` alone does nothing without a valid Dell
  model ID supplied by the corresponding machine.

**Historical and capability boundary:** the 1990s Dell System 425E/433E and
PowerLine models are **not assumed to have used modern F2/F12 hotkeys**.
This is a compatibility convenience, not a reconstruction of their original
ROM UI or a claim that SeaBIOS runs a genuine Dell BIOS. There is no Dell
password system, writable CMOS configuration menu, EISA Configuration Utility,
service-tag programming, or full OEM POST diagnostics. The pre-existing
experimental AMIBIOS CMOS overlay is a separate opt-in and is not enabled by
this mode. Never substitute these labels for actual hardware detection.

F9 boot-menu hotkey (not BIOS Setup)
====================================

QEMU can configure F9 to open the existing SeaBIOS **boot-device menu**:

```sh
qemu-system-i386 -machine pc -fw_cfg name=etc/boot-menu-key,string=C
```

The ASCII byte `C` is `0x43`, the unshifted F9 BIOS scan code. QEMU's
`string=` option writes the bytes of its argument, not a parsed integer:
`string=0x43` **does not** select F9. For clarity and portability, the
equivalent portable POSIX shell form uses octal 103 to write a single byte:

```sh
printf '\103' > f9-boot-key.bin
qemu-system-i386 -machine pc -fw_cfg name=etc/boot-menu-key,file=f9-boot-key.bin
```

When there is no `etc/boot-menu-message` override, the prompt now names F9
rather than incorrectly advertising ESC. ESC remains the default activation
key when `etc/boot-menu-key` is absent; existing custom menu messages win.
The boot-menu wait window still applies.

SeaBIOS does not implement an AMIBIOS Setup program. On various physical PCs,
F9 can choose boot devices or restore defaults *within* a vendor's setup UI.
Do not interpret this optional SeaBIOS boot-menu binding as those capabilities.

