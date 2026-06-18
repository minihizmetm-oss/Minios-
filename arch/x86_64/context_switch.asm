; =============================================================================
; MiniOS Kernel v4.0 - Context Switch
; =============================================================================

BITS 64

global context_switch_asm

context_switch_asm:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov [rdi + 0x30], rsp
    mov rax, [rsp]
    mov [rdi + 0x38], rax

    mov rax, [rsi + 0x50]
    cmp rax, [rdi + 0x50]
    je .same_cr3
    mov cr3, rax
.same_cr3:

    mov rsp, [rsi + 0x30]
    mov rax, [rsi + 0x38]
    push rax

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ret
