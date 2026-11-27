# CPU code generation policy for SeaBIOS, independent of ROM layout.
# The generic default is intentionally unchanged.
BIOS_CPU ?= i386

ifeq ($(BIOS_CPU),i386)
BIOS_CPU_CFLAGS := -march=i386
else ifeq ($(BIOS_CPU),pentium)
BIOS_CPU_CFLAGS := -march=pentium -mtune=pentium -mno-mmx -mno-sse -mno-sse2
else ifeq ($(BIOS_CPU),pentium-mmx)
BIOS_CPU_CFLAGS := -march=pentium-mmx -mtune=pentium-mmx -mno-mmx -mno-sse -mno-sse2
else
$(error Unsupported BIOS_CPU: use i386, pentium or pentium-mmx)
endif

# Keep compiler-generated code from implicitly using MMX/SSE state in BIOS
# interrupt services. Preserve the existing stack and calling conventions.
ifneq ($(config-only),y)
ifneq ($(BIOS_CPU),i386)
bios-cpu-probe := $(shell printf '%s\n' 'int bios_cpu_probe(int x) { return x + 1; }' | \
    $(CC) -m32 $(BIOS_CPU_CFLAGS) -ffreestanding -Os -Werror \
    -x c -c - -o /dev/null >/dev/null 2>&1 && printf '%s' yes)
ifneq ($(bios-cpu-probe),yes)
$(error Compiler cannot generate firmware code for BIOS_CPU=$(BIOS_CPU))
endif
endif
endif
