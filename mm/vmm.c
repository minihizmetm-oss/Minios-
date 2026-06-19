#include "../include/types.h"

extern void *kmalloc(size_t s);
extern void kfree(void *p, size_t s);

#define PAGE_SIZE 4096
#define PAGE_PRESENT (1ULL << 0)
#define PAGE_RW      (1ULL << 1)

typedef uint64_t pte_t;
#define PTE_COUNT 512

static pte_t *kernel_pml4;
static uint64_t heap_current = 0xD0000000;

static pte_t *create_table(void) {
    pte_t *table = kmalloc(PAGE_SIZE);
    if(table) for(int i=0; i<PTE_COUNT; i++) table[i] = 0;
    return table;
}

int map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;
    
    if(!(kernel_pml4[pml4_idx] & PAGE_PRESENT)) {
        pte_t *pdpt = create_table();
        if(!pdpt) return -1;
        kernel_pml4[pml4_idx] = ((uint64_t)pdpt) | PAGE_PRESENT | PAGE_RW;
    }
    pte_t *pdpt = (pte_t *)(kernel_pml4[pml4_idx] & ~0xFFF);
    
    if(!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        pte_t *pd = create_table();
        if(!pd) return -1;
        pdpt[pdpt_idx] = ((uint64_t)pd) | PAGE_PRESENT | PAGE_RW;
    }
    pte_t *pd = (pte_t *)(pdpt[pdpt_idx] & ~0xFFF);
    
    if(!(pd[pd_idx] & PAGE_PRESENT)) {
        pte_t *pt = create_table();
        if(!pt) return -1;
        pd[pd_idx] = ((uint64_t)pt) | PAGE_PRESENT | PAGE_RW;
    }
    pte_t *pt = (pte_t *)(pd[pd_idx] & ~0xFFF);
    
    pt[pt_idx] = (phys & ~0xFFF) | (flags & 0xFFF) | PAGE_PRESENT;
    return 0;
}

void *vm_alloc(uint64_t size) {
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint64_t addr = heap_current;
    heap_current += size;
    for(uint64_t a = addr; a < addr + size; a += PAGE_SIZE) {
        void *phys = kmalloc(PAGE_SIZE);
        if(!phys) return NULL;
        map_page(a, (uint64_t)phys, PAGE_PRESENT | PAGE_RW);
    }
    return (void *)addr;
}

void vmm_init(void) {
    kernel_pml4 = create_table();
    if(!kernel_pml4) return;
    for(uint64_t a=0; a<0x1000000; a+=PAGE_SIZE) map_page(a,a,PAGE_PRESENT|PAGE_RW);
}
