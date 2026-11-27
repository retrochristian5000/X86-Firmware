# SeaBIOS build system
#
# Copyright (C) 2008-2012  Kevin O'Connor <kevin@koconnor.net>
#
# This file may be distributed under the terms of the GNU LGPLv3 license.

# Output directory
OUT=out/

# Common command definitions
HOSTCC ?= cc
export HOSTCC
export CONFIG_SHELL       := sh
export KCONFIG_AUTOHEADER := autoconf.h
export KCONFIG_CONFIG     := $(CURDIR)/.config
export LC_ALL             := C
CROSS_PREFIX=
ifneq ($(CROSS_PREFIX),)
CC=$(CROSS_PREFIX)gcc
endif
AS=$(CROSS_PREFIX)as
NASM?=nasm
NASMFLAGS?=
LD=$(CROSS_PREFIX)ld
OBJCOPY=$(CROSS_PREFIX)objcopy
OBJDUMP=$(CROSS_PREFIX)objdump
STRIP=$(CROSS_PREFIX)strip
PYTHON?=python3
CPP=cpp
LD32BIT_FLAG:=-melf_i386
# The firmware and its intermediate ELF objects never execute code on a stack.
# Preserve the fixed ROM load layout; this only controls ELF stack metadata.
LD_NOEXECSTACK:=-z noexecstack

# Source files
SRCBOTH=misc.c stacks.c output.c string.c block.c cdrom.c disk.c	\
    mouse.c kbd.c system.c serial.c sercon.c clock.c resume.c		\
    pnpbios.c pnpnodes.c vgahooks.c pcibios.c apm.c cp437.c hw/pci.c hw/timer.c	\
    hw/rtc.c hw/dma.c hw/pic.c hw/ps2port.c hw/serialio.c hw/usb.c	\
    hw/usb-uhci.c hw/usb-ohci.c hw/usb-ehci.c hw/usb-hid.c		\
    hw/usb-msc.c hw/usb-uas.c hw/blockcmd.c hw/floppy.c hw/ata.c	\
    hw/ramdisk.c hw/lsi-scsi.c hw/esp-scsi.c hw/megasas.c		\
    hw/mpt-scsi.c
SRC16=$(SRCBOTH)
SRC32FLAT=$(SRCBOTH) post.c e820map.c malloc.c romfile.c x86.c		\
    optionroms.c pmm.c font.c boot.c bootsplash.c jpeg.c bmp.c pcx.c	\
    tcgbios.c sha1.c hw/pcidevice.c hw/ahci.c hw/pvscsi.c		\
    hw/usb-xhci.c hw/usb-hub.c hw/sdcard.c fw/coreboot.c		\
    fw/lzmadecode.c fw/multiboot.c fw/csm.c fw/biostables.c		\
    fw/paravirt.c fw/shadow.c fw/pciinit.c fw/smm.c fw/smp.c		\
    fw/mtrr.c fw/xen.c fw/acpi.c fw/mptable.c fw/pirtable.c		\
    fw/smbios.c fw/romfile_loader.c fw/dsdt_parser.c hw/virtio-ring.c	\
    hw/virtio-pci.c hw/virtio-mmio.c hw/virtio-blk.c hw/virtio-scsi.c	\
    hw/tpm_drivers.c hw/nvme.c hw/sb16-post.c sha256.c sha512.c
SRC32SEG=string.c output.c pcibios.c apm.c stacks.c hw/pci.c hw/serialio.c
DIRS=src src/hw src/fw vgasrc

# Configuration-only commands do not need a working firmware toolchain.
config-only := $(if $(MAKECMDGOALS),$(if $(filter-out %onfig menuconfig-build help,$(MAKECMDGOALS)),,y))

include $(dir $(lastword $(MAKEFILE_LIST)))scripts/cpu-profile.mk

# Default compiler flags
cc-option=$(shell if test -z "`$(1) $(2) -S -o /dev/null -xc /dev/null 2>&1`" \
    ; then echo "$(2)"; else echo "$(3)"; fi ;)

EXTRAVERSION=

CPPFLAGS = -P -MD -MT $@

COMMONCFLAGS := -I$(OUT) -Isrc -Os -MD -g \
    -Wall -Wno-strict-aliasing -Wold-style-definition \
    $(call cc-option,$(CC),-Wtype-limits,) \
    -m32 $(BIOS_CPU_CFLAGS) -mregparm=3 -mpreferred-stack-boundary=2 \
    -minline-all-stringops -fomit-frame-pointer \
    -freg-struct-return -ffreestanding -fno-delete-null-pointer-checks \
    -ffunction-sections -fdata-sections -fno-common -fno-merge-constants
COMMONCFLAGS += $(call cc-option,$(CC),-nopie,)
COMMONCFLAGS += $(call cc-option,$(CC),-fno-pie,)
COMMONCFLAGS += $(call cc-option,$(CC),-fno-stack-protector,)
COMMONCFLAGS += $(call cc-option,$(CC),-fno-stack-protector-all,)
COMMONCFLAGS += $(call cc-option,$(CC),-fstack-check=no,)
COMMONCFLAGS += $(call cc-option,$(CC),-Wno-address-of-packed-member,)
COMMONCFLAGS += $(call cc-option,$(CC),-Wno-array-bounds,)
COMMONCFLAGS += $(call cc-option,$(CC),-fcf-protection=none,)
COMMA := ,

CFLAGS32FLAT := $(COMMONCFLAGS) -DMODE16=0 -DMODESEGMENT=0
# GCC's deferred argument cleanup control is unsupported by Clang.  Probe
# instead of passing an ignored flag or substituting an unrelated LLVM pass.
CFLAGSSEG := $(COMMONCFLAGS) -DMODESEGMENT=1 \
    $(call cc-option,$(CC),-fno-defer-pop,) \
    $(call cc-option,$(CC),-fno-jump-tables,-DMANUAL_NO_JUMP_TABLE) \
    $(call cc-option,$(CC),-fno-tree-switch-conversion,)
CFLAGS32SEG := $(CFLAGSSEG) -DMODE16=0

# Validate actual object generation: -S does not exercise the assembler
# and can accept an unusable .code16gcc fallback.
cc16-option = $(shell printf '%s\n' 'unsigned probe(unsigned x) { return x + 1; }' | \
    $(CC) -m32 $(BIOS_CPU_CFLAGS) -mregparm=3 -ffreestanding -Os -Werror \
    $(1) -x c -c -o /dev/null - >/dev/null 2>&1 && printf '%s' '$(1)')
ifneq ($(config-only),y)
CFLAGS16_MODE := $(call cc16-option,-m16)
ifeq ($(CFLAGS16_MODE),)
CFLAGS16_MODE := $(call cc16-option,-Wa$(COMMA)src/code16gcc.s)
endif
ifeq ($(CFLAGS16_MODE),)
$(error Compiler cannot assemble 16-bit firmware using -m16 or the .code16gcc fallback)
endif
endif

# GCC's stack-frame tuning is optional. Do not disable ordinary inlining
# on compilers that lack it: that changes real-mode calls and stack usage.
CFLAGS16 := $(CFLAGSSEG) -DMODE16=1 \
    $(CFLAGS16_MODE) \
    $(call cc-option,$(CC),--param large-stack-frame=4,)

# Run with "make V=1" to see the actual compile commands
ifdef V
Q=
else
Q=@
MAKEFLAGS += --no-print-directory
endif

# Default targets
ifneq ($(config-only),y)
-include $(KCONFIG_CONFIG)
endif

# Optional size experiments for the 128 KiB BIOS only.  Keep the 16-bit
# and 32-bit segmented code, VGA BIOS, and compiler ABI unchanged.
ROM_FLAT_SIZE_PROFILE ?= default
ROM_FLAT_SIZE_CFLAGS :=
ifneq ($(config-only),y)
ifneq ($(ROM_FLAT_SIZE_PROFILE),default)
ifneq ($(CONFIG_ROM_SIZE),128)
$(error ROM_FLAT_SIZE_PROFILE requires CONFIG_ROM_SIZE=128)
endif
ifneq ($(CONFIG_QEMU),y)
$(error ROM_FLAT_SIZE_PROFILE requires CONFIG_QEMU=y)
endif
ifeq ($(ROM_FLAT_SIZE_PROFILE),switch)
ROM_FLAT_SIZE_CFLAGS := -fno-jump-tables
else ifeq ($(ROM_FLAT_SIZE_PROFILE),loops)
ROM_FLAT_SIZE_CFLAGS := -fno-unroll-loops
else ifeq ($(ROM_FLAT_SIZE_PROFILE),switch-loops)
ROM_FLAT_SIZE_CFLAGS := -fno-jump-tables -fno-unroll-loops
else
$(error Unknown ROM_FLAT_SIZE_PROFILE: use default, switch, loops, or switch-loops)
endif
# Require successful code generation, not just driver acceptance.  Reject
# unsupported experiments instead of silently reporting baseline results.
rom-flat-probe := $(shell printf '%s\n' 'int rom_size_probe(int x) { return x + 1; }' | \
    $(CC) -m32 $(BIOS_CPU_CFLAGS) -ffreestanding -Os -Werror $(ROM_FLAT_SIZE_CFLAGS) \
    -x c -c - -o /dev/null >/dev/null 2>&1 && printf '%s' yes)
ifneq ($(rom-flat-probe),yes)
$(error Compiler cannot generate i386 code with $(ROM_FLAT_SIZE_CFLAGS))
endif
endif
endif

target-y :=
target-$(CONFIG_QEMU) += $(OUT)bios.bin
target-$(CONFIG_CSM) += $(OUT)Csm16.bin
target-$(CONFIG_COREBOOT) += $(OUT)bios.bin.elf
target-$(CONFIG_BUILD_VGABIOS) += $(OUT)vgabios.bin

all: $(target-y)

# Make definitions
.PHONY : all clean distclean FORCE
.DELETE_ON_ERROR:


################ Common build rules

# Verify the build environment works for firmware builds only.
CFLAGSWHOLE ?=
ifneq ($(config-only),y)
TESTGCC:=$(shell OUT="$(OUT)" CC="$(CC)" LD="$(LD)" scripts/test-build.sh)
ifeq "$(TESTGCC)" "-1"
$(error "Please upgrade the build environment")
endif

ifeq "$(TESTGCC)" "0"
# Use -fwhole-program
CFLAGSWHOLE=-fwhole-program -DWHOLE_PROGRAM
endif
endif

# Do a whole file compile by textually including all C code.
define whole-compile
@echo "  Compiling whole program $3"
$(Q)printf '$(foreach i,$2,#include "$i"\n)' > $3.tmp.c
$(Q)$(CC) -I. $1 $(CFLAGSWHOLE) -c $3.tmp.c -o $3
endef

%.noexec.o: %.o scripts/ldnoexec.py
	@echo "  Stripping $@"
	$(Q)$(STRIP) $< -o $<.strip.o
	$(Q)$(PYTHON) ./scripts/ldnoexec.py $<.strip.o $@

# Track effective firmware compilation settings, not just the CPU profile.
# Mixed 16-bit and 32-bit objects can otherwise survive a compiler/ABI change.
profile-quote = '$(subst ','"'"',$(1))'
$(OUT)bios-cpu.profile: FORCE
	$(Q)set -eu; umask 077; mkdir -p "$(OUT)"; tmp=$(mktemp "$@.XXXXXX"); \
		trap 'rm -f "$tmp"' 0; trap 'exit 1' HUP INT TERM; \
		printf '%s\n' \
			$(call profile-quote,BIOS_CPU=$(BIOS_CPU)) \
			$(call profile-quote,BIOS_CPU_CFLAGS=$(BIOS_CPU_CFLAGS)) \
			$(call profile-quote,CC=$(CC)) \
			$(call profile-quote,AS=$(AS)) \
			$(call profile-quote,CFLAGS16=$(CFLAGS16)) \
			$(call profile-quote,CFLAGS32SEG=$(CFLAGS32SEG)) \
			$(call profile-quote,CFLAGS32FLAT=$(CFLAGS32FLAT)) \
			$(call profile-quote,CFLAGSWHOLE=$(CFLAGSWHOLE)) > "$tmp"; \
		if ! cmp -s "$tmp" "$@"; then mv "$tmp" "$@"; fi

$(OUT)%.s: %.c $(OUT)bios-cpu.profile
	@echo "  Compiling to assembler $@"
	$(Q)$(CC) $(CFLAGS16) -S -c $< -o $@

$(OUT)%.o: %.c $(OUT)autoconf.h $(OUT)bios-cpu.profile
	@echo "  Compile checking $@"
	$(Q)$(CC) $(CFLAGS32FLAT) -c $< -o $@

# NASM sources are explicitly .asm so GAS-oriented .S and compiler-generated
# assembly keep their existing toolchain. SeaBIOS objects are always ELF32/i386.
$(OUT)%.o: %.asm
	@echo "  Assembling (NASM) $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(NASM) -f elf32 $(NASMFLAGS) -o $@ $<

$(OUT)%.lds: %.lds.S
	@echo "  Precompiling $@"
	$(Q)$(CPP) $(CPPFLAGS) -D__ASSEMBLY__ $< -o $@


################ Main BIOS build rules

$(OUT)asm-offsets.s: $(OUT)autoconf.h

$(OUT)asm-offsets.h: $(OUT)src/asm-offsets.s
	@echo "  Generating offset file $@"
	$(Q)./scripts/gen-offsets.sh $< $@

$(OUT)ccode16.o: $(OUT)autoconf.h $(OUT)bios-cpu.profile $(patsubst %.c, $(OUT)src/%.o,$(SRC16)) ; $(call whole-compile, $(CFLAGS16), $(addprefix src/, $(SRC16)),$@)

$(OUT)code32seg.o: $(OUT)autoconf.h $(OUT)bios-cpu.profile $(patsubst %.c, $(OUT)src/%.o,$(SRC32SEG)) ; $(call whole-compile, $(CFLAGS32SEG), $(addprefix src/, $(SRC32SEG)),$@)

# Track the selected flat compiler flags independently of the linker profile.
$(OUT)rom-flat-cflags.profile: FORCE
	$(Q)set -eu; umask 077; mkdir -p "$(OUT)"; tmp=$$(mktemp "$@.XXXXXX"); \
		trap 'rm -f "$$tmp"' 0; \
		trap 'exit 1' HUP INT TERM; \
		printf '%s\n' "$(ROM_FLAT_SIZE_PROFILE)" "$(ROM_FLAT_SIZE_CFLAGS)" > "$$tmp"; \
		if ! cmp -s "$$tmp" "$@"; then mv "$$tmp" "$@"; fi

$(OUT)ccode32flat.o: $(OUT)autoconf.h $(OUT)bios-cpu.profile $(OUT)rom-flat-cflags.profile $(patsubst %.c, $(OUT)src/%.o,$(SRC32FLAT)) ; $(call whole-compile, $(CFLAGS32FLAT) $(ROM_FLAT_SIZE_CFLAGS), $(addprefix src/, $(SRC32FLAT)),$@)

$(OUT)romlayout.o: src/romlayout.S $(OUT)autoconf.h $(OUT)bios-cpu.profile $(OUT)asm-offsets.h
	@echo "  Compiling (16bit) $@"
	$(Q)$(CC) $(CFLAGS16) -c -D__ASSEMBLY__ $< -o $@

# Keep the intermediate links independent of linker-script generation.
# A layout-only change must not rebuild the whole-program ELF inputs.
$(OUT)autoversion.h: src/version.c scripts/buildversion.py $(OUT)ccode32flat.o $(OUT)ccode16.o $(OUT)code32seg.o $(OUT)romlayout.o
	@echo "  Building BIOS version"
	$(Q)$(PYTHON) ./scripts/buildversion.py -e "$(EXTRAVERSION)" -t "$(CC);$(AS);$(LD);$(OBJCOPY);$(OBJDUMP);$(STRIP)" $@

$(OUT)version.o: src/version.c $(OUT)autoversion.h
	$(Q)$(CC) $(CFLAGS32FLAT) -c $< -o $@

$(OUT)code32flat.o: $(OUT)ccode32flat.o $(OUT)version.o
	@echo "  Linking $@"
	$(Q)$(LD) $(LD32BIT_FLAG) -r $^ -o $@

$(OUT)code16.o: $(OUT)ccode16.o $(OUT)romlayout.o
	@echo "  Linking $@"
	$(Q)$(LD) $(LD32BIT_FLAG) -r $^ -o $@

# Track placement policy without recompiling unchanged firmware inputs.
# Auto selects the standard QEMU layout for a fixed 128 or 256 KiB ROM.
# Specialized machine layouts and optional text packing remain explicit.
ROM_LAYOUT_VARIANT ?= auto
ROM_LAYOUT_PACK ?= 0
$(OUT)romlayout.profile: FORCE
	$(Q)set -e; umask 077; tmp=$$(mktemp "$@.XXXXXX"); \
		trap 'rm -f "$$tmp"' EXIT; \
		printf '%s\n' "$(ROM_LAYOUT_VARIANT)" "$(ROM_LAYOUT_PACK)" > "$$tmp"; \
		if ! cmp -s "$$tmp" "$@"; then mv "$$tmp" "$@"; fi

# Multi-target pattern rules are grouped by GNU make, including older
# versions that do not support the explicit &: grouped-target syntax.
# Each invocation produces all three scripts, even if just one was missing.
$(OUT)romlayout16.% $(OUT)romlayout32seg.% $(OUT)romlayout32flat.%: $(OUT)code32flat.o $(OUT)code32seg.o $(OUT)code16.o $(OUT)$(KCONFIG_AUTOHEADER) $(OUT)romlayout.profile scripts/layoutrom.py scripts/layoutprofiles.py
	@echo "  Building ld scripts"
	$(Q)$(OBJDUMP) -thr $(OUT)code32flat.o > $(OUT)code32flat.o.objdump
	$(Q)$(OBJDUMP) -thr $(OUT)code32seg.o > $(OUT)code32seg.o.objdump
	$(Q)$(OBJDUMP) -thr $(OUT)code16.o > $(OUT)code16.o.objdump
	$(Q)WHP_ROM_LAYOUT_VARIANT="$(ROM_LAYOUT_VARIANT)" WHP_ROM_LAYOUT_PACK="$(ROM_LAYOUT_PACK)" $(PYTHON) ./scripts/layoutrom.py $(OUT)code16.o.objdump $(OUT)code32seg.o.objdump $(OUT)code32flat.o.objdump $(OUT)$(KCONFIG_AUTOHEADER) $(OUT)romlayout16.lds $(OUT)romlayout32seg.lds $(OUT)romlayout32flat.lds

# Every mode shares cross-mode addresses. Wait for all generated scripts,
# including recovery of a missing peer, before linking any mode.
ROM_LAYOUT_SCRIPTS := $(OUT)romlayout16.lds $(OUT)romlayout32seg.lds $(OUT)romlayout32flat.lds

$(OUT)rom16.o: $(OUT)code16.o $(ROM_LAYOUT_SCRIPTS)
	@echo "  Linking $@"
	$(Q)$(LD) $(LD_NOEXECSTACK) -e 0 -T $(OUT)romlayout16.lds $< -o $@

$(OUT)rom32seg.o: $(OUT)code32seg.o $(ROM_LAYOUT_SCRIPTS)
	@echo "  Linking $@"
	$(Q)$(LD) $(LD_NOEXECSTACK) -e 0 -T $(OUT)romlayout32seg.lds $< -o $@

$(OUT)rom.o: $(OUT)rom16.noexec.o $(OUT)rom32seg.noexec.o $(OUT)code32flat.o $(ROM_LAYOUT_SCRIPTS)
	@echo "  Linking $@"
	$(Q)$(LD) $(LD_NOEXECSTACK) -N -T $(OUT)romlayout32flat.lds $(OUT)rom16.noexec.o $(OUT)rom32seg.noexec.o $(OUT)code32flat.o -o $@

$(OUT)bios.bin.prep: $(OUT)rom.o scripts/checkrom.py
	@echo "  Prepping $@"
	$(Q)rm -f $(OUT)bios.bin $(OUT)Csm16.bin $(OUT)bios.bin.elf
	$(Q)$(OBJDUMP) -thr $< > $<.objdump
	$(Q)$(OBJCOPY) -O binary $< $(OUT)bios.bin.raw
	$(Q)$(PYTHON) ./scripts/checkrom.py $<.objdump $(CONFIG_ROM_SIZE) $(OUT)bios.bin.raw $(OUT)bios.bin.prep

$(OUT)bios.bin: $(OUT)bios.bin.prep
	@echo "  Creating $@"
	$(Q)cp $< $@

$(OUT)Csm16.bin: $(OUT)bios.bin.prep
	@echo "  Creating $@"
	$(Q)cp $< $@

$(OUT)bios.bin.elf: $(OUT)rom.o $(OUT)bios.bin.prep
	@echo "  Creating $@"
	$(Q)$(STRIP) -R .comment $< -o $(OUT)bios.bin.elf


# Compare the actual ROM payload, not the ELF container, without modifying
# rom.o or requiring checkrom.py to accept an oversized image first.
.PHONY: rom-strip-audit
rom-strip-audit: $(OUT)rom.o
	$(Q)set -eu; umask 077; tmp=$$(mktemp -d "$(OUT)rom-strip-audit.XXXXXX"); \
		trap 'rm -f "$$tmp/original.bin" "$$tmp/stripped.o" "$$tmp/stripped.bin"; rmdir "$$tmp"' 0; \
		trap 'exit 1' HUP INT TERM; \
		$(OBJCOPY) -O binary "$<" "$$tmp/original.bin"; \
		$(STRIP) --strip-all -R .comment "$<" -o "$$tmp/stripped.o"; \
		$(OBJCOPY) -O binary "$$tmp/stripped.o" "$$tmp/stripped.bin"; \
		printf 'ELF bytes: %d -> %d\n' "$$(wc -c < "$<")" "$$(wc -c < "$$tmp/stripped.o")"; \
		printf 'Raw ROM bytes: %d -> %d\n' "$$(wc -c < "$$tmp/original.bin")" "$$(wc -c < "$$tmp/stripped.bin")"; \
		if ! cmp -s "$$tmp/original.bin" "$$tmp/stripped.bin"; then \
			printf '%s\n' 'Error: strip changed the firmware payload; inspect the ELF section layout.' >&2; \
			exit 1; \
		fi; \
		printf '%s\n' 'Strip removed no raw ROM bytes; the firmware payload is unchanged.'

################ VGA build rules

# VGA src files
SRCVGA=src/output.c src/string.c src/hw/pci.c src/hw/serialio.c \
    vgasrc/vgainit.c vgasrc/vgabios.c vgasrc/vgafb.c vgasrc/swcursor.c \
    vgasrc/vgafonts.c vgasrc/vbe.c \
    vgasrc/stdvga.c vgasrc/stdvgamodes.c vgasrc/stdvgaio.c \
    vgasrc/clext.c vgasrc/svgamodes.c vgasrc/atiext.c vgasrc/bochsvga.c vgasrc/geodevga.c \
    src/fw/coreboot.c vgasrc/cbvga.c vgasrc/bochsdisplay.c vgasrc/ramfb.c

ifeq "$(CONFIG_VGA_FIXUP_ASM)" "y"
$(OUT)vgaccode16.raw.s: $(OUT)autoconf.h $(OUT)bios-cpu.profile $(patsubst %.c, $(OUT)%.o,$(SRCVGA)) ; $(call whole-compile, $(filter-out -fomit-frame-pointer,$(CFLAGS16)) -fno-omit-frame-pointer -S -Isrc, $(SRCVGA),$@)

$(OUT)vgaccode16.o: $(OUT)vgaccode16.raw.s scripts/vgafixup.py
	@echo "  Fixup VGA rom assembler"
	$(Q)$(PYTHON) ./scripts/vgafixup.py $< $(OUT)vgaccode16.s
	$(Q)$(AS) --32 src/code16gcc.s $(OUT)vgaccode16.s -o $@
else
$(OUT)vgaccode16.o: $(OUT)autoconf.h $(OUT)bios-cpu.profile $(patsubst %.c, $(OUT)%.o,$(SRCVGA)) ; $(call whole-compile, $(CFLAGS16) -Isrc, $(SRCVGA),$@)
endif

$(OUT)vgaentry.o: vgasrc/vgaentry.S $(OUT)autoconf.h $(OUT)bios-cpu.profile $(OUT)asm-offsets.h
	@echo "  Compiling (16bit) $@"
	$(Q)$(CC) $(CFLAGS16) -c -D__ASSEMBLY__ $< -o $@

$(OUT)vgarom.o: $(OUT)vgaccode16.o $(OUT)vgaentry.o $(OUT)vgasrc/vgalayout.lds vgasrc/vgaversion.c scripts/buildversion.py
	@echo "  Linking $@"
	$(Q)$(PYTHON) ./scripts/buildversion.py -e "$(EXTRAVERSION)" -t "$(CC);$(AS);$(LD);$(OBJCOPY);$(OBJDUMP);$(STRIP)" $(OUT)autovgaversion.h
	$(Q)$(CC) $(CFLAGS16) -c vgasrc/vgaversion.c -o $(OUT)vgaversion.o
	$(Q)$(LD) $(LD_NOEXECSTACK) --gc-sections -T $(OUT)vgasrc/vgalayout.lds $(OUT)vgaccode16.o $(OUT)vgaentry.o $(OUT)vgaversion.o -o $@

$(OUT)vgabios.bin.raw: $(OUT)vgarom.o
	@echo "  Extracting binary $@"
	$(Q)$(OBJCOPY) -O binary $< $@

$(OUT)vgabios.bin: $(OUT)vgabios.bin.raw scripts/buildrom.py
	@echo "  Finalizing rom $@"
	$(Q)$(PYTHON) ./scripts/buildrom.py $< $@


################ Kconfig rules

define do-kconfig
$(Q)mkdir -p $(OUT)/scripts/kconfig/lxdialog
$(Q)mkdir -p $(OUT)/include/config
$(Q)mkdir -p $(addprefix $(OUT), $(DIRS))
$(Q)$(MAKE) -C $(OUT) -f $(CURDIR)/scripts/kconfig/Makefile srctree=$(CURDIR) src=scripts/kconfig obj=scripts/kconfig Q=$(Q) Kconfig=$(CURDIR)/src/Kconfig $1
endef

$(OUT)autoconf.h : $(KCONFIG_CONFIG) ; $(call do-kconfig, silentoldconfig)
$(KCONFIG_CONFIG): src/Kconfig vgasrc/Kconfig ; $(call do-kconfig, olddefconfig)
%onfig: ; $(call do-kconfig, $@)
.PHONY: menuconfig-build
menuconfig-build: ; $(call do-kconfig, $@)
help: ; $(call do-kconfig, $@)


################ Generic rules

clean:
	$(Q)rm -rf $(OUT)

distclean: clean
	$(Q)rm -f .config .config.old

-include $(OUT)*.d $(patsubst %,$(OUT)%/*.d,$(DIRS))
