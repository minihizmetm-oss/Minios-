#include "../include/types.h"

#define PAGE_SIZE 4096

static uint8_t *pool;
static uint32_t total_pages;
static uint32_t free_pages;
static uint32_t *bitmap;

void pmm_init(uint64_t mem_start, uint64_t mem_end) {
    pool = (uint8_t *)mem_start;
    total_pages = (mem_end - mem_start) / PAGE_SIZE;
    free_pages = total_pages;
    bitmap = (uint32_t *)pool;
    uint32_t bm_pages = (total_pages + 31) / 32;
    for(uint32_t i=0; i<bm_pages; i++) bitmap[i] = 0;
    pool += bm_pages * PAGE_SIZE;
    free_pages -= bm_pages;
}

void *kmalloc(size_t size) {
    uint32_t needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t start = 0, count = 0;
    for(uint32_t i=0; i<total_pages; i++) {
        uint32_t word = i/32, bit = i%32;
        if(!(bitmap[word] & (1u<<bit))) {
            if(count == 0) start = i;
            count++;
            if(count == needed) {
                for(uint32_t j=0; j<needed; j++) {
                    uint32_t w = (start+j)/32, b = (start+j)%32;
                    bitmap[w] |= (1u<<b);
                }
                free_pages -= needed;
                return pool + start * PAGE_SIZE;
            }
        } else count = 0;
    }
    return NULL;
}

void kfree(void *ptr, size_t size) {
    if(!ptr) return;
    uint32_t start = ((uint8_t *)ptr - pool) / PAGE_SIZE;
    uint32_t needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    for(uint32_t i=0; i<needed; i++) {
        uint32_t w = (start+i)/32, b = (start+i)%32;
        bitmap[w] &= ~(1u<<b);
    }
    free_pages += needed;
}
