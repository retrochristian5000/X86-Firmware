/* Verify BIOS floppy DMA cleanup and repeat transfers on PIIX.
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
    movb %dl, drive
    /* Leave channel 1 available, channel 2 initially masked. */
    movb $1, %al
    outb %al, $0x0a
    inb $0x0f, %al
    movb %al, saved_mask
    inb $0xde, %al
    movb %al, saved_high
    movb $2, attempts
read_again:
    movw $0, 0x8000
    movw $0x8000, %bx
    movw $0x0201, %ax
    movw $2, %cx
    xorw %dx, %dx
    movb drive, %dl
    int $0x13
    jc fail
    cmpw $0xa55a, 0x8000
    jne fail
    call check_masks
    decb attempts
    jnz read_again
    /* Read-only image: write must fail, but DMA must be cleaned up. */
    movw $0x8000, %bx
    movw $0x0301, %ax
    movw $2, %cx
    xorw %dx, %dx
    movb drive, %dl
    int $0x13
    jnc fail
    call check_masks
    movb $0x10, %al
    outb %al, $0xf4
    hlt
check_masks:
    inb $0x0f, %al
    movb saved_mask, %ah
    orb $4, %ah
    cmpb %ah, %al
    jne fail
    inb $0xde, %al
    cmpb saved_high, %al
    jne fail
    ret
fail:
    movb $0x20, %al
    outb %al, $0xf4
    hlt
drive:
    .byte 0
saved_mask:
    .byte 0
saved_high:
    .byte 0
attempts:
    .byte 0
.org 510
.word 0xaa55
