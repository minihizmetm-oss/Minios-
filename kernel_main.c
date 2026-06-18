#include "types.h"
#include "printk.h"

typedef struct task_struct {
    int pid; int tid; int ppid; char name[16]; int state; int prio; int nice;
    uint64_t vruntime; uint64_t exec_start; uint64_t sum_exec_runtime;
    uint64_t rsp; uint64_t rip; uint64_t rflags; uint64_t cr3;
    uint64_t *kernel_stack; struct task_struct *next; struct task_struct *prev;
    struct task_struct *parent; uint64_t stack_canary; float predicted_cpu_usage;
} task_struct_t;

int strlen(const char *s) { int i=0; while(s[i])i++; return i; }
int strcmp(const char *a, const char *b) { while(*a&&*b&&*a==*b){a++;b++;} return *a-*b; }

extern void printk_init(void);
extern void gdt_init(void);
extern void idt_init(void);
extern void timer_init(void);
extern void keyboard_init(void);
extern void pmm_init(uint64_t a, uint64_t b);
extern void vmm_init(void);
extern void scheduler_init(void);
extern void ai_engine_init(void);
extern void security_init(void);
extern task_struct_t *create_task(const char *name, void (*entry)(void));
extern char keyboard_getc(void);

void kernel_early_init(uint64_t magic, uint64_t multiboot_info) {}

void kernel_main(void) {
    // COM1 port'una direkt yaz
    char *msg = "\n\n========================================\n";
    char *msg2 = "  MiniOS Kernel v4.0 BOOTED!\n";
    char *msg3 = "========================================\n\n";
    
    char *p = msg;
    while(*p) { while(!(__inb(0x3FD) & 0x20)); __outb(0x3F8, *p++); }
    p = msg2;
    while(*p) { while(!(__inb(0x3FD) & 0x20)); __outb(0x3F8, *p++); }
    p = msg3;
    while(*p) { while(!(__inb(0x3FD) & 0x20)); __outb(0x3F8, *p++); }
    
    while(1) __hlt();
}
