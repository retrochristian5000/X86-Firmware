/* Cirrus palette mode-transition regression; linked at 0x7c00.
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
    movw %ax, %es
    sti
    cld

    /* VBE RGB555 must not select the mixed palette mode. */
    movw $0x4f02, %ax
    movw $0x0110, %bx
    int $0x10
    cmpw $0x004f, %ax
    jne fail_hdr
    call read_hdr
    cmpb $0xe0, %al
    jne fail_hdr

    /* Set indexed mode, then retain a custom DAC entry and mask. */
    movw $0x005f, %ax
    int $0x10
    movw $0x1201, %ax
    movw $0x0031, %bx
    int $0x10
    cmpb $0x12, %al
    jne fail_preserve
    movw $0x3c8, %dx
    movb $0x2a, %al
    outb %al, %dx
    incw %dx
    movb $0x03, %al
    outb %al, %dx
    movb $0x15, %al
    outb %al, %dx
    movb $0x2f, %al
    outb %al, %dx
    movw $0x3c6, %dx
    movb $0x7f, %al
    outb %al, %dx

    /* Leave cursor LUT selected and HDR unlocked as a prior driver could. */
    movw $0x3c4, %dx
    movw $0x8212, %ax
    outw %ax, %dx
    movw $0x3c6, %dx
    inb %dx, %al
    inb %dx, %al
    inb %dx, %al
    inb %dx, %al
    movw $0x005c, %ax
    int $0x10
    call read_hdr
    testb %al, %al
    jne fail_hdr
    inb %dx, %al
    cmpb $0x7f, %al
    jne fail_preserve
    movw $0x3c4, %dx
    movb $0x12, %al
    outb %al, %dx
    incw %dx
    inb %dx, %al
    testb $0x83, %al
    jnz fail_preserve
    movb $0x2a, %al
    call read_rgb
    cmpb $0x03, %bl
    jne fail_preserve
    cmpb $0x15, %bh
    jne fail_preserve
    cmpb $0x2f, %al
    jne fail_preserve

    /* Enable default palette loading and grayscale for a new mode. */
    movw $0x1200, %ax
    movw $0x0031, %bx
    int $0x10
    movw $0x1200, %ax
    movw $0x0033, %bx
    int $0x10
    movw $0x005f, %ax
    int $0x10
    movb $1, %al
    call read_rgb
    cmpb %bl, %bh
    jne fail_gray
    cmpb %bl, %al
    jne fail_gray
    testb %al, %al
    jz fail_gray

    /* Disabling grayscale must restore the ordinary color palette. */
    movw $0x1201, %ax
    movw $0x0033, %bx
    int $0x10
    movw $0x005f, %ax
    int $0x10
    movb $1, %al
    call read_rgb
    cmpb %bl, %al
    je fail_gray
    movb $0x10, %al
    jmp exit_test

/* Return HDR in AL, DX=3c6 with the unlock sequence consumed. */
read_hdr:
    movw $0x3c8, %dx
    xorb %al, %al
    outb %al, %dx
    movw $0x3c6, %dx
    inb %dx, %al
    inb %dx, %al
    inb %dx, %al
    inb %dx, %al
    inb %dx, %al
    ret

/* AL=index; return RGB in BL, BH, AL. */
read_rgb:
    movw $0x3c7, %dx
    outb %al, %dx
    addw $2, %dx
    inb %dx, %al
    movb %al, %bl
    inb %dx, %al
    movb %al, %bh
    inb %dx, %al
    ret

fail_hdr:
    movb $0x20, %al
    jmp exit_test
fail_preserve:
    movb $0x21, %al
    jmp exit_test
fail_gray:
    movb $0x22, %al
exit_test:
    outb %al, $0xf4
    cli
    hlt
.org 510
.word 0xaa55
