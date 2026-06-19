#include "../include/types.h"

extern void *kmalloc(size_t s);
extern void kfree(void *p, size_t s);

/* ============================================================
 * SCHEDULER - AI Destekli CFS + MLFQ
 * ============================================================ */

#define MAX_TASKS 256
#define SCHED_QUANTUM 10
#define NICE_MIN -20
#define NICE_MAX 19
#define MLFQ_LEVELS 4
#define PRIO_REALTIME 0
#define PRIO_HIGH 40
#define PRIO_NORMAL 80
#define PRIO_LOW 120
#define PRIO_IDLE 139

/* Task durumları */
typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_BLOCKED,
    TASK_ZOMBIE,
    TASK_DEAD
} task_state_t;

/* Task yapısı */
typedef struct task {
    int pid;
    int tid;
    int ppid;
    char name[32];
    task_state_t state;
    int priority;
    int nice;
    int mlfq_level;
    
    /* CFS alanları */
    uint64_t vruntime;
    uint64_t exec_start;
    uint64_t sum_exec_runtime;
    uint64_t last_ran;
    
    /* Uyku */
    uint64_t sleep_until;
    uint64_t wake_time;
    
    /* CPU */
    int cpu;
    uint64_t cpu_time;
    
    /* AI Tahminleri */
    float predicted_cpu;
    float predicted_io;
    float load_score;
    
    /* Stack ve context */
    void *kernel_stack;
    uint64_t entry_point;
    uint64_t rsp, rip, rflags;
    
    /* IPC */
    int exit_code;
    int wait_pid;
    
    /* Listeler */
    struct task *next;
    struct task *prev;
    struct task *parent;
    struct task *children;
    struct task *sibling;
} task_t;

/* MLFQ kuyrukları */
static task_t *mlfq[MLFQ_LEVELS];
static int mlfq_quantum[MLFQ_LEVELS] = {5, 10, 25, 50};

/* Global değişkenler */
static task_t *task_list = NULL;
static task_t *current_task = NULL;
static task_t *idle_task = NULL;
static int next_pid = 1;
static int num_tasks = 0;
static uint64_t system_ticks = 0;

/* ============================================================
 * YARDIMCI FONKSİYONLAR
 * ============================================================ */

static void list_add(task_t **head, task_t *task) {
    task->next = *head;
    task->prev = NULL;
    if(*head) (*head)->prev = task;
    *head = task;
}

static void list_remove(task_t **head, task_t *task) {
    if(task->prev) task->prev->next = task->next;
    else *head = task->next;
    if(task->next) task->next->prev = task->prev;
    task->next = task->prev = NULL;
}

static int str_len(const char *s) {
    int i=0; while(s[i]) i++; return i;
}

static void str_cpy(char *dst, const char *src, int max) {
    int i;
    for(i=0; i<max-1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

/* ============================================================
 * AI CPU TAHMİNİ
 * ============================================================ */

static float ai_predict_cpu(task_t *task) {
    /* Basit EMA (Exponential Moving Average) */
    float alpha = 0.3f;
    uint64_t runtime = task->sum_exec_runtime;
    float actual = (float)(runtime % 100) / 100.0f;
    task->predicted_cpu = alpha * actual + (1.0f - alpha) * task->predicted_cpu;
    task->load_score = task->predicted_cpu * 10.0f;
    return task->predicted_cpu;
}

static float ai_predict_io(task_t *task) {
    /* Basit I/O tahmini */
    task->predicted_io = task->predicted_cpu * 0.5f;
    return task->predicted_io;
}

/* ============================================================
 * MLFQ YÖNETİMİ
 * ============================================================ */

static int mlfq_calc_level(int priority, int nice) {
    int level = 0;
    if(priority < PRIO_HIGH) level = 0;
    else if(priority < PRIO_NORMAL) level = 1;
    else if(priority < PRIO_LOW) level = 2;
    else level = 3;
    
    if(nice > 0 && level < MLFQ_LEVELS-1) level++;
    if(nice < 0 && level > 0) level--;
    
    return level;
}

static void mlfq_enqueue(task_t *task) {
    int level = task->mlfq_level;
    list_add(&mlfq[level], task);
}

static task_t *mlfq_dequeue(int level) {
    if(!mlfq[level]) return NULL;
    task_t *task = mlfq[level];
    list_remove(&mlfq[level], task);
    return task;
}

static void mlfq_demote(task_t *task) {
    if(task->mlfq_level < MLFQ_LEVELS - 1) {
        task->mlfq_level++;
    }
}

static void mlfq_promote(task_t *task) {
    if(task->mlfq_level > 0) {
        task->mlfq_level--;
    }
}

/* ============================================================
 * SCHEDULER ÇEKİRDEĞİ
 * ============================================================ */

/* Boş task - CPU hiçbir şey yapmazken çalışır */
static void idle_loop(void) {
    while(1) {
        __asm__ __volatile__("hlt");
    }
}

/* Task oluştur */
task_t *create_task(const char *name, void (*entry)(void), int priority) {
    if(num_tasks >= MAX_TASKS) return NULL;
    
    task_t *task = kmalloc(sizeof(task_t));
    if(!task) return NULL;
    
    /* Sıfırla */
    for(int i=0; i<sizeof(task_t); i++) ((uint8_t*)task)[i] = 0;
    
    task->pid = next_pid++;
    task->tid = task->pid;
    task->state = TASK_READY;
    task->priority = priority;
    task->nice = 0;
    task->mlfq_level = mlfq_calc_level(priority, 0);
    task->entry_point = (uint64_t)entry;
    task->cpu = 0;
    task->predicted_cpu = 0.5f;
    task->predicted_io = 0.2f;
    task->load_score = 1.0f;
    
    str_cpy(task->name, name, 31);
    
    /* Kernel stack ayır */
    task->kernel_stack = kmalloc(4096);
    if(task->kernel_stack) {
        task->rsp = (uint64_t)task->kernel_stack + 4096 - 64;
    }
    
    /* Listeye ekle */
    list_add(&task_list, task);
    mlfq_enqueue(task);
    num_tasks++;
    
    return task;
}

/* Task'i fork et */
task_t *fork_task(task_t *parent) {
    task_t *child = create_task(parent->name, (void(*)(void))parent->entry_point, parent->priority);
    if(child) {
        child->ppid = parent->pid;
        child->parent = parent;
        child->sibling = parent->children;
        parent->children = child;
    }
    return child;
}

/* Task'i sonlandır */
void kill_task(task_t *task) {
    if(!task || task->state == TASK_DEAD) return;
    
    task->state = TASK_ZOMBIE;
    
    /* Çocukları yetim bırak */
    task_t *child = task->children;
    while(child) {
        child->parent = NULL;
        child = child->sibling;
    }
    
    /* Belleği temizle */
    if(task->kernel_stack) {
        kfree(task->kernel_stack, 4096);
    }
    
    task->state = TASK_DEAD;
    num_tasks--;
}

/* En iyi sonraki task'i seç */
task_t *pick_next_task(void) {
    task_t *next = NULL;
    
    /* MLFQ: en yüksek öncelikli seviyeden başla */
    for(int level = 0; level < MLFQ_LEVELS; level++) {
        task_t *best = NULL;
        uint64_t min_vruntime = ~0ULL;
        float best_score = 999.0f;
        
        task_t *t = mlfq[level];
        while(t) {
            if(t->state == TASK_READY) {
                /* AI destekli seçim: vruntime + predicted_cpu */
                float score = (float)t->vruntime + t->predicted_cpu * 50.0f;
                
                if(score < best_score || (score == best_score && t->vruntime < min_vruntime)) {
                    best_score = score;
                    min_vruntime = t->vruntime;
                    best = t;
                }
            }
            t = t->next;
        }
        
        if(best) {
            list_remove(&mlfq[level], best);
            return best;
        }
    }
    
    /* Hiç task yoksa idle'a dön */
    if(!next && idle_task && idle_task->state == TASK_READY) {
        next = idle_task;
    }
    
    return next;
}

/* Ana scheduler fonksiyonu */
void schedule(void) {
    system_ticks++;
    
    /* Mevcut task'i durdur */
    if(current_task && current_task->state == TASK_RUNNING) {
        current_task->state = TASK_READY;
        
        /* CPU zamanını güncelle */
        current_task->sum_exec_runtime++;
        current_task->vruntime += (100 + current_task->nice * 5);
        
        /* AI tahminini güncelle */
        ai_predict_cpu(current_task);
        ai_predict_io(current_task);
        
        /* MLFQ: quantum dolduysa seviye düşür */
        if((current_task->sum_exec_runtime % (uint64_t)mlfq_quantum[current_task->mlfq_level]) == 0) {
            mlfq_demote(current_task);
        }
        
        /* Kuyruğa geri ekle */
        mlfq_enqueue(current_task);
    }
    
    /* Sonraki task'i seç */
    task_t *next = pick_next_task();
    
    if(next) {
        /* I/O bekleyen task'i yükselt */
        if(next->state == TASK_BLOCKED && next->sum_exec_runtime < 5) {
            mlfq_promote(next);
        }
        
        next->state = TASK_RUNNING;
        next->exec_start = system_ticks;
        next->last_ran = system_ticks;
        next->cpu_time++;
        
        current_task = next;
    }
}

/* Timer tick */
void scheduler_tick(void) {
    system_ticks++;
    
    /* Uyuyan task'leri kontrol et */
    task_t *t = task_list;
    while(t) {
        if(t->state == TASK_SLEEPING && system_ticks >= t->wake_time) {
            t->state = TASK_READY;
            mlfq_enqueue(t);
        }
        t = t->next;
    }
    
    /* Quantum kontrolü */
    if(current_task && current_task->state == TASK_RUNNING) {
        uint64_t elapsed = system_ticks - current_task->exec_start;
        if(elapsed >= mlfq_quantum[current_task->mlfq_level]) {
            schedule();
        }
    }
}

/* Task'i uyut */
void sleep_task(uint64_t ms) {
    if(current_task) {
        current_task->state = TASK_SLEEPING;
        current_task->wake_time = system_ticks + ms;
        schedule();
    }
}

/* Scheduler başlat */
void scheduler_init(void) {
    /* MLFQ kuyruklarını temizle */
    for(int i=0; i<MLFQ_LEVELS; i++) mlfq[i] = NULL;
    
    task_list = NULL;
    current_task = NULL;
    next_pid = 1;
    num_tasks = 0;
    system_ticks = 0;
    
    /* Idle task oluştur */
    idle_task = create_task("idle", idle_loop, PRIO_IDLE);
    if(idle_task) {
        idle_task->pid = 0;
        current_task = idle_task;
        idle_task->state = TASK_RUNNING;
    }
}

/* Task listesini göster */
void list_tasks(void) {
    volatile char *serial = (volatile char *)0x3F8;
    char *hdr = "\n[PID] NAME                STATE     PRI  CPU   VRUNTIME\n";
    while(*hdr) *serial = *hdr++;
    
    task_t *t = task_list;
    while(t) {
        char buf[80];
        char *states[] = {"READY", "RUN", "SLEEP", "BLOCK", "ZOMBIE", "DEAD"};
        
        buf[0]='['; buf[1]='0'+t->pid/100; buf[2]='0'+(t->pid/10)%10; buf[3]='0'+t->pid%10; buf[4]=']'; buf[5]=' ';
        for(int i=0; i<20; i++) buf[6+i] = t->name[i] ? t->name[i] : ' ';
        buf[26]=' ';
        char *st = states[t->state];
        for(int i=0; i<6; i++) buf[27+i] = st[i] ? st[i] : ' ';
        buf[33]=' ';
        buf[34]='0'+t->priority/100; buf[35]='0'+(t->priority/10)%10; buf[36]='0'+t->priority%10;
        buf[37]=' ';
        buf[38]='0'+t->cpu_time/100; buf[39]='0'+(t->cpu_time/10)%10; buf[40]='0'+t->cpu_time%10;
        buf[41]=' ';
        buf[42]='\n';
        
        for(int i=0; i<43; i++) *serial = buf[i];
        t = t->next;
    }
}

/* Task'e öncelik ver */
void task_set_priority(task_t *task, int priority) {
    if(!task) return;
    task->priority = priority;
    task->mlfq_level = mlfq_calc_level(priority, task->nice);
}

/* Task'e nice değeri ver */
void task_set_nice(task_t *task, int nice) {
    if(!task || nice < NICE_MIN || nice > NICE_MAX) return;
    task->nice = nice;
    task->mlfq_level = mlfq_calc_level(task->priority, nice);
}
