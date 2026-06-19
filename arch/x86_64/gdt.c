#include "../include/types.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __packed;

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __packed;

static struct gdt_entry gdt[3];
static struct gdt_ptr gdt_ptr;

void gdt_init(void) {
    gdt[0].limit_low=0; gdt[0].base_low=0; gdt[0].base_mid=0;
    gdt[0].access=0; gdt[0].granularity=0; gdt[0].base_high=0;
    
    gdt[1].limit_low=0xFFFF; gdt[1].base_low=0; gdt[1].base_mid=0;
    gdt[1].access=0x9A; gdt[1].granularity=0xCF; gdt[1].base_high=0;
    
    gdt[2].limit_low=0xFFFF; gdt[2].base_low=0; gdt[2].base_mid=0;
    gdt[2].access=0x92; gdt[2].granularity=0xCF; gdt[2].base_high=0;
    
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;
    
    __asm__ __volatile__("lgdt %0" : : "m"(gdt_ptr));
}
