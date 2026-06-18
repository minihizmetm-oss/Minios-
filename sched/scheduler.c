#include "../include/types.h"

typedef struct task_struct {
    int pid;
    char name[16];
    int state;
    struct task_struct *next;
    uint64_t stack_canary;
} task_struct_t;

task_struct_t *current_task = NULL;

void scheduler_init(void) {}

task_struct_t *create_task(const char *name, void (*entry)(void)) {
    return NULL;
}

void schedule(void) {}
