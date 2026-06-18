BITS 32

; PVH ELF Note (QEMU'nun istediği format)
section .note.PVH
align 8
    dd 4           ; namesz
    dd 20          ; descsz
    dd 0x00100000  ; type: XEN_ELFNOTE_PHYS32_ENTRY
    db "Xen", 0
    dd 0x00100000  ; entry point
    dd 0           ; flags
    dd 0           ; hypercall page
    dd 0           ; hw_compat
    dd 0           ; feature_flags

; Multiboot 2 header
section .multiboot2
align 8
mb_start:
    dd 0xe85250d6           ; magic
    dd 0                    ; arch
    dd mb_end - mb_start    ; length
    dd -(0xe85250d6 + 0 + (mb_end - mb_start))  ; checksum
    
    ; End tag
    dw 0
    dw 0
    dd 8
mb_end:

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
