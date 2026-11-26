/* Boot-sector regression for PCI BIOS 2.1 Get IRQ Routing Options.
 * GNU-compatible i386 assembler; linked at 0x7c00.
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
    cld

    /* Real-mode query: one extra slot (device 10), seven total. */
    movw $0, 0x600
    movw $0x700, 0x602
    movw $0, 0x604
    movw $0x600, %di
    movw $0xb10e, %ax
    xorw %bx, %bx
    pushw %ds
    movw $0xf000, %dx
    movw %dx, %ds
    int $0x1a
    popw %ds
    jnc fail16
    cmpb $0x89, %ah
    jne fail16
    cmpw $112, 0x600
    jne fail16
    movw $0xb10e, %ax
    xorw %bx, %bx
    pushw %ds
    movw $0xf000, %dx
    movw %dx, %ds
    int $0x1a
    popw %ds
    jc fail16
    cmpw $0x5000, 0x760
    jne fail16
    cmpb $0x61, 0x762
    jne fail16
    cmpw $0xdef8, 0x763
    jne fail16

    /* Locate the BIOS32 directory in the F-segment. */
    movw $0xf000, %ax
    movw %ax, %es
    xorw %di, %di
1:
    cmpl $0x5f32335f, %es:(%di)
    je 2f
    addw $16, %di
    jnc 1b
    jmp fail16
2:
    movl %es:4(%di), %eax
    movl %eax, bios32_ptr
    lgdt gdt_desc
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmpl $8, $protected

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
    movl $0x49435024, %eax
    xorl %ebx, %ebx
    lcall *bios32_ptr
    testb %al, %al
    jne fail32
    addl %ebx, %edx
    movl %edx, pci_ptr

    /* Both parameter and output offsets exceed 64 KiB. */
    movw $0, 0x12000
    movl $0x14000, 0x12002
    movw $16, 0x12006
    movl $0x12000, %edi
    movw $0xb10e, %ax
    xorl %ebx, %ebx
    lcall *pci_ptr
    jnc fail32
    cmpb $0x89, %ah
    jne fail32
    cmpw $112, 0x12000
    jne fail32
    movl $0x12000, %edi
    movw $0xb10e, %ax
    xorl %ebx, %ebx
    lcall *pci_ptr
    jc fail32
    cmpw $112, 0x12000
    jne fail32

    /* The two entry modes must return identical routing entries. */
    movl $0x700, %esi
    movl $0x14000, %edi
    movl $112, %ecx
    cld
    repe cmpsb
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
bios32_ptr:
    .long 0
    .word 8
pci_ptr:
    .long 0
    .word 8
.org 510
.word 0xaa55
