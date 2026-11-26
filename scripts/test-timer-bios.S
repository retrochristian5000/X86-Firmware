/* Real-mode RTC wait regression, linked at 0x7c00. */
/* SPDX-License-Identifier: LGPL-3.0-or-later */
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

    /* Count hardware IRQ8 deliveries and chain to the original BIOS ISR. */
    movl 0x1c0, %eax
    movl %eax, old70
    movw $irq70, 0x1c0
    movw $0, 0x1c2

    /* The BIOS uses a rounded 977-us interval per RTC interrupt. */
    movw $977, %dx
    movw $1, %bp
    call wait_test
    movw $978, %dx
    movw $2, %bp
    call wait_test
    movw $1954, %dx
    movw $2, %bp
    call wait_test

    /* Restore the IRQ vector and verify standard time/date services. */
    movl old70, %eax
    movl %eax, 0x1c0
    sti
    movw $0x0200, %ax
    int $0x1a
    jc fail
    movw $0x0400, %ax
    int $0x1a
    jc fail

    movb $0x10, %al
    outb %al, $0xf4
    cli
    hlt

wait_test:
    cli
    movb $0, done_flag
    movw $0, irq_count
    pushw %bp
    xorw %cx, %cx
    movw $done_flag, %bx
    movw $0x8300, %ax
    int $0x15
    popw %bp
    jc fail
1:
    cli
    testb $0x80, done_flag
    jnz 2f
    sti
    hlt
    jmp 1b
2:
    cmpw %bp, irq_count
    jne fail
    ret

irq70:
    incw %cs:irq_count
    ljmp *%cs:old70

fail:
    movb $0x20, %al
    outb %al, $0xf4
    cli
    hlt

old70:
    .long 0
irq_count:
    .word 0
done_flag:
    .byte 0
.org 510
.word 0xaa55
