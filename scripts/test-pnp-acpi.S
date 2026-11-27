/* ACPI and PnP BIOS runtime publication must not overlap.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
.section .text
.code16
.global _start
_start:
    cli
    xorw %ax, %ax
    movw %ax, %ss
    movw $0x7c00, %sp
    movw %ax, %ds
    cld

    /* PnP BIOS 1.0A installation structures are aligned in F0000-FFFFF. */
    movw $0xf000, %ax
    movw %ax, %es
    xorw %di, %di
pnp_scan:
    cmpl $0x506e5024, %es:(%di)
    jne pnp_next
    cmpw $0x2110, %es:4(%di)
    jne pnp_next
    movw %di, %si
    movw $33, %cx
    call sum_bytes
    testb %al, %al
    jz fail
pnp_next:
    addw $16, %di
    cmpw $0xffd0, %di
    jbe pnp_scan

    /* QEMU publishes its ACPI RSDP in the BIOS area. Require its presence
     * rather than accepting a boot with neither enumeration interface. */
    movw $0xe000, %dx
rsdp_segment:
    movw %dx, %es
    xorw %di, %di
rsdp_scan:
    cmpl $0x20445352, %es:(%di)
    jne rsdp_next
    cmpl $0x20525450, %es:4(%di)
    jne rsdp_next
    movw %di, %si
    movw $20, %cx
    call sum_bytes
    testb %al, %al
    jz pass
rsdp_next:
    addw $16, %di
    cmpw $0xffe0, %di
    jbe rsdp_scan
    cmpw $0xf000, %dx
    je fail
    movw $0xf000, %dx
    jmp rsdp_segment

sum_bytes:
    xorb %al, %al
1:
    addb %es:(%si), %al
    incw %si
    loop 1b
    ret

pass:
    movb $0x10, %al
    outb %al, $0xf4
    hlt
fail:
    movb $0x20, %al
    outb %al, $0xf4
    hlt

.org 510
.word 0xaa55
