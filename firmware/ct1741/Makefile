# SPDX-License-Identifier: LGPL-3.0-or-later
# Standalone MCU build. Never linked into the parent x86 firmware.
SDCC ?= sdcc
MAKEBIN ?= makebin
HOSTCC ?= cc
HOSTCFLAGS ?= -O2
ROM_SIZE := 8192
MCUFLAGS := -mmcs51 --model-small --iram-size 256 --code-size $(ROM_SIZE)

.PHONY: all check clean
all: build/ct1741.bin

build:
	mkdir -p $@

build/command.rel: command.c command.h Makefile | build
	cd build && $(SDCC) $(MCUFLAGS) -c ../command.c

build/main.rel: main.c command.h Makefile | build
	cd build && $(SDCC) $(MCUFLAGS) -c ../main.c

build/ct1741.ihx: build/main.rel build/command.rel
	cd build && $(SDCC) $(MCUFLAGS) -o ct1741.ihx main.rel command.rel

build/ct1741.bin: build/ct1741.ihx
	$(MAKEBIN) -s $(ROM_SIZE) < $< > $@.tmp
	test "$$(wc -c < $@.tmp)" -eq $(ROM_SIZE)
	mv $@.tmp $@

build/test-command: test-command.c command.c command.h Makefile | build
	$(HOSTCC) $(HOSTCFLAGS) -std=c99 -Wall -Wextra -Werror \
	    -o $@ test-command.c command.c

check: build/test-command
	./build/test-command

clean:
	rm -rf build
