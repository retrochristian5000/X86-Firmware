/* BIOS PS/2 mouse service regression. SPDX-License-Identifier: LGPL-3.0-or-later */
.section .text
.code16
.global _start
_start:
    ljmp $0, $start
start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7c00, %sp
    cld
    sti

    /* Standard three-byte mouse reset and identification. */
    movw $0xc205, %ax
    movw $0x0300, %bx
    int $0x15
    jc fail
    cmpw $0x00aa, %bx
    jne fail
    movw $0xc204, %ax
    int $0x15
    jc fail
    testb %bh, %bh
    jne fail

    /* Install a callback and enable stream reporting. */
    xorw %ax, %ax
    movw %ax, %es
    movw $callback, %bx
    movw $0xc207, %ax
    int $0x15
    jc fail
    movw $0xc200, %ax
    movw $0x0100, %bx
    int $0x15
    jc fail
    movw $0xc206, %ax
    xorw %bx, %bx
    int $0x15
    jc fail
    testb $0x20, %bl
    jz fail

    /* Reset while reporting is enabled must disable it. */
    movw $0xc201, %ax
    int $0x15
    jc fail
    cmpw $0x00aa, %bx
    jne fail
    movw $0xc206, %ax
    xorw %bx, %bx
    int $0x15
    jc fail
    testb $0x20, %bl
    jnz fail
    cmpb $4, %cl
    jne fail
    cmpb $100, %dl
    jne fail

    /* Check the actual 8042 command byte, not just the mouse status. */
    cli
    movw $0xffff, %cx
1:
    inb $0x64, %al
    testb $2, %al
    jz 2f
    loop 1b
    jmp fail
2:
    movb $0x20, %al
    outb %al, $0x64
    movw $0xffff, %cx
3:
    inb $0x64, %al
    testb $1, %al
    jnz 4f
    loop 3b
    jmp fail
4:
    inb $0x60, %al
    andb $0x22, %al
    cmpb $0x20, %al
    jne fail

    movb $0x10, %al
    outb %al, $0xf4
    hlt
fail:
    movb $0x20, %al
    outb %al, $0xf4
    cli
    hlt
callback:
    lret
.org 510
.word 0xaa55
