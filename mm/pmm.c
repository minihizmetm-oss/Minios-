#include "../include/types.h"
void pmm_init(uint64_t a, uint64_t b) {}
void* kmalloc(size_t s) { return (void*)0x100000; }
void kfree(void* p, size_t s) {}
