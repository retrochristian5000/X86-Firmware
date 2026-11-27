# SPDX-License-Identifier: LGPL-3.0-or-later
# Standalone MCU build. Never linked into the parent x86 firmware.
SDCC ?= sdcc
MAKEBIN ?= makebin
HOSTCC ?= cc
HOSTCFLAGS ?= -O2
BUILD_DIR ?= build
OUT := $(abspath $(BUILD_DIR))
SOURCE_DIR := $(CURDIR)
ROM_SIZE := 8192
MCUFLAGS := -mmcs51 --model-small --iram-size 256 --code-size $(ROM_SIZE)

.PHONY: all check clean
all: $(OUT)/ct1741.bin

$(OUT):
	mkdir -p "$@"

$(OUT)/command.rel: command.c command.h Makefile | $(OUT)
	cd "$(OUT)" && $(SDCC) $(MCUFLAGS) -c -o command.rel "$(SOURCE_DIR)/command.c"

$(OUT)/main.rel: main.c command.h Makefile | $(OUT)
	cd "$(OUT)" && $(SDCC) $(MCUFLAGS) -c -o main.rel "$(SOURCE_DIR)/main.c"

$(OUT)/ct1741.ihx: $(OUT)/main.rel $(OUT)/command.rel
	cd "$(OUT)" && $(SDCC) $(MCUFLAGS) -o ct1741.ihx main.rel command.rel

$(OUT)/ct1741.bin: $(OUT)/ct1741.ihx
	$(MAKEBIN) -s $(ROM_SIZE) < "$<" > "$@.tmp"
	test "$$(wc -c < "$@.tmp")" -eq $(ROM_SIZE)
	mv "$@.tmp" "$@"

$(OUT)/test-command: test-command.c command.c command.h Makefile | $(OUT)
	$(HOSTCC) $(HOSTCFLAGS) -std=c99 -Wall -Wextra -Werror \
	    -o "$@" test-command.c command.c

check: $(OUT)/test-command
	"$(OUT)/test-command"

clean:
	# Custom output directories are owned and cleaned by the caller.
	@test "$(OUT)" = "$(CURDIR)/build" || { printf '%s\n' 'error: clean only supports the default CT1741 build directory' >&2; exit 1; }
	rm -rf -- "$(CURDIR)/build"
