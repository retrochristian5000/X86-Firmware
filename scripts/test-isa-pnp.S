/* Real-mode PnP BIOS bus-node and ISA configuration regression.
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
    movw $0xf000, %ax
    movw %ax, %es
    xorw %di, %di
1:
    cmpl $0x506e5024, %es:(%di)
    jne 2f
    cmpw $0x2110, %es:4(%di)
    jne 2f
    movw %di, %si
    movw $0x21, %cx
    xorb %al, %al
3:
    addb %es:(%si), %al
    incw %si
    loop 3b
    testb %al, %al
    je 4f
2:
    addw $16, %di
    jnc 1b
    jmp fail
4:
    movw %es:13(%di), %ax
    movw %ax, entry
    movw %es:15(%di), %ax
    movw %ax, entry+2
    movw %es:27(%di), %ax
    movw %ax, bios_ds

    /* Function 00h: six nodes, largest DMA node (69 bytes). */
    pushw bios_ds
    pushw $0
    pushw $0x602
    pushw $0
    pushw $0x600
    pushw $0
    lcall *entry
    addw $12, %sp
    testw %ax, %ax
    jne fail
    cmpb $6, 0x600
    jne fail
    cmpw $69, 0x602
    jne fail

    /* Function 01h: enumerate all nodes, current and boot configs. */
    movw $1, control
next_config:
    movb $0, 0x604
    call get_node
    testw %ax, %ax
    jne fail
    cmpb $1, 0x604
    jne fail
    cmpw $26, 0x610
    jne fail
    cmpb $0, 0x612
    jne fail
    cmpl $0x030ad041, 0x613
    jne fail
    call get_node
    testw %ax, %ax
    jne fail
    cmpb $2, 0x604
    jne fail
    cmpw $18, 0x610
    jne fail
    cmpb $1, 0x612
    jne fail
    cmpl $0x000ad041, 0x613
    jne fail
    cmpw $3, 0x61a
    jne fail
    cmpw $0x79, 0x61c
    jne fail
    cmpw $0x79, 0x61e
    jne fail
    cmpw $0x79, 0x620
    jne fail
    /* Verify the four fixed motherboard nodes and their sizes/IDs. */
    movw $core_nodes, %si
    movw $4, %cx
core_loop:
    call get_node
    testw %ax, %ax
    jne fail
    lodsw
    cmpw %ax, 0x610
    jne fail
    lodsw
    cmpw %ax, 0x615
    jne fail
    loop core_loop
    cmpb $0xff, 0x604
    jne fail
    call get_node
    cmpw $0x83, %ax
    jne fail
    incw control
    cmpw $3, control
    jne next_config

    /* Function 02h: fixed ISA node rejects changes. */
    pushw bios_ds
    pushw $1
    pushw $0
    pushw $0x610
    pushw $1
    pushw $2
    lcall *entry
    addw $12, %sp
    cmpw $0x85, %ax
    jne fail

    /* Function 40h: exactly six bytes; do not overwrite sentinel. */
    movl $0xffffffff, 0x630
    movl $0xffffffff, 0x634
    pushw bios_ds
    pushw $0
    pushw $0x630
    pushw $0x40
    lcall *entry
    addw $8, %sp
    testw %ax, %ax
    jne fail
    cmpl $1, 0x630
    jne fail
    cmpw $0, 0x634
    jne fail
    cmpw $0xffff, 0x636
    jne fail

    /* A six-byte result cannot fit at offset FFFBh. */
    pushw bios_ds
    pushw $0
    pushw $0xfffb
    pushw $0x40
    lcall *entry
    addw $8, %sp
    cmpw $0x8c, %ax
    jne fail
    movb $0x10, %al
    outb %al, $0xf4
    hlt

get_node:
    pushw bios_ds
    pushw control
    pushw $0
    pushw $0x610
    pushw $0
    pushw $0x604
    pushw $1
    lcall *entry
    addw $14, %sp
    ret

fail:
    movb $0x20, %al
    outb %al, $0xf4
    hlt

entry:
    .long 0
bios_ds:
    .word 0
control:
    .word 0
core_nodes:
    .word 45, 0x0000, 69, 0x0002, 29, 0x0001, 29, 0x000b
.org 510
.word 0xaa55
