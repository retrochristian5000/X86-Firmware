; SPDX-License-Identifier: GPL-2.0-or-later
; Real-mode BIOS caller: nonzero SS, small and high SP, hardware IRQs.
bits 16
org 0x7c00

%macro check_stack 0
    pushf
    pop ax
    and ax, 0x0600
    cmp ax, 0x0600             ; IF and DF must survive the interrupt
    jne fail
    cld
    mov ax, ss
    cmp ax, [stack_seg]
    jne fail
    cmp sp, [stack_top]
    jne fail
    mov bx, sp
    cmp word [ss:bx], 0x1234
    jne fail
    cmp word [ss:bx-256], 0x5678
    jne fail
%endmacro

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov [drive], dl
    mov word [stack_seg], 0x2000
    mov word [stack_top], 0x0200
test_stack:
    cli
    mov ax, [stack_seg]
    mov ss, ax
    mov sp, [stack_top]
    mov bx, sp
    mov word [ss:bx], 0x1234
    mov word [ss:bx-256], 0x5678
    sti
    std
    int 0x12
    cmp ax, 512
    jb fail
    check_stack

    mov ah, 1
    std
    int 0x16
    check_stack

    mov ah, 0
    std
    int 0x1a
    check_stack

    mov ax, 0x8600
    xor cx, cx
    mov dx, 20000
    std
    int 0x15
    jc fail
    check_stack

    mov ax, 0x0201
    mov cx, 2
    xor dh, dh
    mov dl, [drive]
    mov bx, 0x8000
    std
    int 0x13
    jc fail
    check_stack
    cmp word [0x8000], 0xabcd
    jne fail

    ; Invalid full-width E820 continuation must not alias entry zero.
    mov eax, 0xe820
    mov ebx, 0x10000
    mov ecx, 20
    mov edx, 0x534d4150
    mov di, 0x8200
    std
    int 0x15
    jnc fail
    check_stack

    cmp word [stack_seg], 0x3000
    je pass
    mov word [stack_seg], 0x3000
    mov word [stack_top], 0xfff0
    jmp test_stack
pass:
    mov al, 'P'
    jmp finish
fail:
    mov al, 'F'
finish:
    out 0xe9, al
    mov dx, 0xf4
    mov al, 0x10
    out dx, al
    cli
    hlt
drive: db 0
stack_seg: dw 0
stack_top: dw 0
times 510-($-$$) db 0
dw 0xaa55
