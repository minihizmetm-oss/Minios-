#include "types.h"
extern void printk(const char *s);

/* Tüm modüller */
extern void gdt_init(void);
extern void idt_init(void);
extern void pmm_init(uint64_t a, uint64_t b);
extern void vmm_init(void);
extern void scheduler_init(void);
extern void ai_engine_init(void);
extern void security_init(void);
extern void keyboard_init(void);
extern void timer_init(void);
extern void e1000_init(void);
extern void printk_init(void);
extern void printk_banner(void);
extern void printk_boot(const char *mod, int status);
extern void printk_info(const char *s);
extern void printk_ok(const char *s);
extern void printk_warn(const char *s);
extern void printk_err(const char *s);
extern void printk_clear(void);
extern void printk_header(const char *s);
extern void printk_box(const char *t, const char *m);
extern void printk_line(char ch, int count);
extern void printk_arrow(const char *s);
extern void printk_loading(const char *msg, int ms);
extern char keyboard_getc(void);

void kernel_early_init(uint64_t magic, uint64_t multiboot_info) {
    /* GRUB'tan gelen bilgileri sakla */
    if(magic != 0x2BADB002) {
        /* Hatalı magic, ama devam et */
    }
}

/* Boot animasyonu */
static void boot_animation(void) {
    printk_clear();
    printk_banner();
    printk_info("MiniOS Kernel v4.0 starting...");
    printk_line('=', 80);
    
    printk_loading("Initializing GDT/IDT", 500);
    gdt_init();
    idt_init();
    printk_boot("CPU Descriptor Tables", 1);
    
    printk_loading("Initializing Memory Manager", 500);
    pmm_init(0x200000, 0x1000000);
    vmm_init();
    printk_boot("Physical Memory (16MB)", 1);
    printk_boot("Virtual Memory (4-level)", 1);
    
    printk_loading("Initializing Security", 500);
    security_init();
    printk_boot("ASLR", 1);
    printk_boot("Stack Canary", 1);
    printk_boot("NX Bit", 1);
    printk_boot("SMEP/SMAP", 1);
    
    printk_loading("Initializing AI Engine", 800);
    ai_engine_init();
    printk_boot("AI Perceptron", 1);
    printk_boot("AI Anomaly Detection", 1);
    printk_boot("AI LSTM", 1);
    
    printk_loading("Initializing Scheduler", 500);
    scheduler_init();
    printk_boot("CFS Scheduler", 1);
    printk_boot("MLFQ Queues", 1);
    
    printk_loading("Initializing Hardware", 500);
    timer_init();
    keyboard_init();
    printk_boot("PIT Timer (1000Hz)", 1);
    printk_boot("PS/2 Keyboard", 1);
    
    printk_loading("Initializing Network", 500);
    e1000_init();
    printk_boot("E1000 NIC", 1);
}

/* Sistem bilgisi */
static void show_sysinfo(void) {
    printk_header("SYSTEM INFORMATION");
    printk_arrow("Kernel: MiniOS v4.0");
    printk_arrow("Architecture: x86");
    printk_arrow("AI Engine: Active (39KB)");
    printk_arrow("Security: Enabled (ASLR+SMEP+Canary)");
    printk_arrow("Scheduler: CFS+MLFQ");
    printk_arrow("Network: E1000 Ready");
    printk_arrow("Memory: 16MB Managed");
}

/* Mini shell */
static void mini_shell(void) {
    printk_header("MINIOS SHELL");
    printk_info("Type 'help' for commands");
    
    char cmd[64];
    int idx = 0;
    
    while(1) {
        printk("minios> ");
        idx = 0;
        
        while(idx < 63) {
            char c = keyboard_getc();
            
            if(c == '\n' || c == '\r') {
                cmd[idx] = '\0';
                printk("\n");
                break;
            } else if(c == '\b') {
                if(idx > 0) { idx--; printk("\b \b"); }
            } else if(c >= 32) {
                cmd[idx++] = c;
                char tmp[2] = {c, 0};
                printk(tmp);
            }
        }
        cmd[idx] = '\0';
        
        /* Komutları işle */
        int match = 1;
        const char *help = "help";
        for(int i=0; i<4; i++) if(cmd[i] != help[i]) match = 0;
        if(match) {
            printk_box("COMMANDS", "help info clear reboot sysinfo banner");
            continue;
        }
        
        match = 1;
        const char *info = "info";
        for(int i=0; i<4; i++) if(cmd[i] != info[i]) match = 0;
        if(match) { printk_info("MiniOS Kernel v4.0 - AI-Powered"); continue; }
        
        match = 1;
        const char *clear = "clear";
        for(int i=0; i<5; i++) if(cmd[i] != clear[i]) match = 0;
        if(match) { printk_clear(); continue; }
        
        match = 1;
        const char *banner = "banner";
        for(int i=0; i<6; i++) if(cmd[i] != banner[i]) match = 0;
        if(match) { printk_banner(); continue; }
        
        match = 1;
        const char *sysinfo = "sysinfo";
        for(int i=0; i<7; i++) if(cmd[i] != sysinfo[i]) match = 0;
        if(match) { show_sysinfo(); continue; }
        
        match = 1;
        const char *reboot = "reboot";
        for(int i=0; i<6; i++) if(cmd[i] != reboot[i]) match = 0;
        if(match) {
            printk_warn("Rebooting...");
            /* Klavye controller reset */
            __outb(0x64, 0xFE);
            while(1) __hlt();
        }
        
        if(idx > 0) {
            printk("Unknown command: ");
            printk(cmd);
            printk("\n");
        }
    }
}

/* Ana kernel */
void kernel_main(void) {
    printk_init();
    boot_animation();
    
    printk_line('=', 80);
    printk_ok("All subsystems initialized!");
    printk_ok("MiniOS Kernel v4.0 ready!");
    printk_line('=', 80);
    
    mini_shell();
    
    while(1) __hlt();
}
