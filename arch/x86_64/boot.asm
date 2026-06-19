BITS 32
section .multiboot
align 4
    dd 0x1BADB002
    dd 0x00000003
    dd -(0x1BADB002 + 0x00000003)
section .text
global _start
_start:
    mov esp, 0x200000
    push eax
    push ebx
    extern kernel_main
    call kernel_main
    cli
    hlt
