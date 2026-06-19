#include "../include/types.h"
struct isr_context { uint64_t dummy[20]; };
void idt_init(void) {}
void isr_dispatch(struct isr_context *ctx) {}
void idt_register_handler(int v, void *h) {}
void irq_unmask(int irq) {}
