#include "../include/types.h"

struct isr_context {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

void idt_init(void) {}

void isr_dispatch(struct isr_context *ctx) {}

void idt_register_handler(int v, void *h) {}

void irq_unmask(int irq) {}
