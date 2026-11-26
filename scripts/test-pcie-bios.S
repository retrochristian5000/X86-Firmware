/* Q35 ECAM and bridge bus allocation regression. */
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
    movw %ax, %ss
    movw $0x7c00, %sp

    /* Root port 00:02.0 must forward both buses 1 and 2. */
    movl $0x80001018, %eax
    call cam_read
    andl $0x00ffffff, %eax
    cmpl $0x00020100, %eax
    jne fail16
    /* PCIe-to-PCI bridge 01:00.0 forwards bus 2. */
    movl $0x80010018, %eax
    call cam_read
    andl $0x00ffffff, %eax
    cmpl $0x00020201, %eax
    jne fail16
    /* Next root port must not reuse bus 2. */
    movl $0x80001818, %eax
    call cam_read
    andl $0x00ffffff, %eax
    cmpl $0x00030300, %eax
    jne fail16
    movl $0x80020800, %eax
    call cam_read
    cmpl $0x100e8086, %eax
    jne fail16
    movl $0x80000060, %eax
    call cam_read
    cmpl $0xb0000001, %eax
    jne fail16

    /* Flat protected-mode data segment for direct ECAM reads. */
    inb $0x92, %al
    andb $0xfe, %al
    orb $2, %al
    outb %al, $0x92
    lgdt gdt_desc
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmpl $8, $protected

cam_read:
    movw $0xcf8, %dx
    outl %eax, %dx
    movw $0xcfc, %dx
    inl %dx, %eax
    ret
fail16:
    movb $0x20, %al
    outb %al, $0xf4
    hlt

.code32
protected:
    movw $16, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movl $0x7c00, %esp
    /* ECAM exposes the same conventional identity. */
    cmpl $0x100e8086, 0xb0208000
    jne fail32
    /* Root-port AER capability is at extended offset 100h. */
    cmpw $1, 0xb0010100
    jne fail32
    movb $0x10, %al
    outb %al, $0xf4
    hlt
fail32:
    movb $0x21, %al
    outb %al, $0xf4
    hlt

.align 8
gdt:
    .quad 0
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
gdt_desc:
    .word 23
    .long gdt
.org 510
.word 0xaa55
