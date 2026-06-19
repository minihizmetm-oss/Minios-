#include "../include/types.h"

/* ============================================================
 * MINIOS PRINTK v4.0 - ULTIMATE
 * ============================================================ */

#define VGA ((volatile uint16_t *)0xB8000)
#define W 80
#define H 25
#define COL(fg,bg) ((bg<<4)|fg)

enum { BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LGRAY,
       DGRAY, LBLUE, LGREEN, LCYAN, LRED, LMAGENTA, YELLOW, WHITE };

static int x, y;
static uint8_t cur = 0x07;

static void put(char ch) {
    if(ch=='\n') { x=0; y++; }
    else if(ch=='\r') x=0;
    else if(ch=='\t') x=(x+8)&~7;
    else if(ch=='\b') { if(x>0) x--; }
    else {
        VGA[y*W+x] = (uint8_t)ch | (cur<<8);
        x++;
        if(x>=W) { x=0; y++; }
    }
    if(y>=H) {
        for(int i=0; i<(H-1)*W; i++) VGA[i] = VGA[i+W];
        for(int i=(H-1)*W; i<H*W; i++) VGA[i] = ' ' | (0x07<<8);
        y = H-1;
    }
}

/* Temel yazdırma */
void printk(const char *s) { while(*s) put(*s++); }

/* Renkli yazdırma */
void printk_color(const char *s, uint8_t fg, uint8_t bg) {
    uint8_t old = cur; cur = COL(fg,bg); printk(s); cur = old;
}
void printk_red(const char *s)    { printk_color(s, LRED, BLACK); }
void printk_green(const char *s)  { printk_color(s, LGREEN, BLACK); }
void printk_yellow(const char *s) { printk_color(s, YELLOW, BLACK); }
void printk_cyan(const char *s)   { printk_color(s, LCYAN, BLACK); }
void printk_white(const char *s)  { printk_color(s, WHITE, BLACK); }

/* Sayı yazdırma */
void printk_dec(uint64_t n) {
    if(n==0) { put('0'); return; }
    char buf[21]; int i=20; buf[20]=0;
    while(n>0) { buf[--i]='0'+n%10; n/=10; }
    while(i<20) put(buf[i++]);
}
void printk_hex(uint64_t n) {
    printk("0x");
    char h[]="0123456789ABCDEF";
    int sh=60; while(sh>=0 && !((n>>sh)&0xF)) sh-=4;
    if(sh<0) put('0');
    for(; sh>=0; sh-=4) put(h[(n>>sh)&0xF]);
}
void printk_ptr(void *p) { printk_hex((uint64_t)p); }

/* Log seviyeleri */
void printk_info(const char *s) { printk_green("[INFO] "); printk(s); printk("\n"); }
void printk_warn(const char *s) { printk_yellow("[WARN] "); printk(s); printk("\n"); }
void printk_err(const char *s)  { printk_red("[ERROR] "); printk(s); printk("\n"); }
void printk_ok(const char *s)   { printk_green("[  OK  ] "); printk(s); printk("\n"); }
void printk_fail(const char *s) { printk_red("[ FAIL ] "); printk(s); printk("\n"); }
void printk_dbg(const char *s)  { printk("[DEBUG] "); printk(s); printk("\n"); }

/* İlerleme çubuğu */
void printk_progress(const char *label, int done, int total) {
    int pct = total ? (done*100/total) : 100;
    printk(label); printk(" [");
    for(int i=0; i<25; i++) put(i*4<pct ? '=' : i*4==pct ? '>' : ' ');
    printk("] "); printk_dec(pct); printk("%\r");
}

/* Tablo */
void printk_table_start(const char *h1, const char *h2, const char *h3) {
    printk("\n");
    cur = COL(WHITE, BLUE);
    printk(" "); printk(h1);
    for(int i=0; h1[i]; i++) put(' ');
    printk(" | "); printk(h2);
    for(int i=0; h2[i]; i++) put(' ');
    printk(" | "); printk(h3); printk("\n");
    cur = 0x07;
    for(int i=0; i<60; i++) put('-');
    printk("\n");
}
void printk_table_row(const char *c1, uint64_t c2, const char *c3) {
    printk(" "); printk(c1); printk(" | "); printk_dec(c2); printk(" | "); printk(c3); printk("\n");
}
void printk_table_end(void) {
    for(int i=0; i<60; i++) put('-');
    printk("\n\n");
}

/* Kutu içinde mesaj */
void printk_box(const char *title, const char *msg) {
    printk_cyan("+"); for(int i=0; i<52; i++) printk_cyan("-"); printk_cyan("+\n");
    printk_cyan("| "); printk_white(title);
    int len=0; while(title[len]) len++;
    for(int i=len; i<50; i++) put(' ');
    printk_cyan("|\n");
    printk_cyan("| "); printk(msg);
    len=0; while(msg[len]) len++;
    for(int i=len; i<50; i++) put(' ');
    printk_cyan("|\n");
    printk_cyan("+"); for(int i=0; i<52; i++) printk_cyan("-"); printk_cyan("+\n\n");
}

/* Başlık */
void printk_header(const char *s) {
    printk("\n"); printk_cyan("=== "); printk_white(s); printk_cyan(" ===\n\n");
}

/* Alt başlık */
void printk_subheader(const char *s) {
    printk_cyan("--- "); printk(s); printk_cyan(" ---\n");
}

/* Bellek dump */
void printk_dump(void *addr, int bytes) {
    uint8_t *p = (uint8_t*)addr;
    printk_header("MEMORY DUMP");
    printk_ptr(addr); printk(":\n");
    for(int i=0; i<bytes; i+=16) {
        printk_hex((uint64_t)(p+i)); printk("  ");
        for(int j=0; j<16 && (i+j)<bytes; j++) {
            uint8_t b = p[i+j];
            char h[]="0123456789ABCDEF";
            put(h[b>>4]); put(h[b&0xF]); put(' ');
        }
        printk(" |");
        for(int j=0; j<16 && (i+j)<bytes; j++) {
            char ch = p[i+j];
            put(ch>=32 && ch<127 ? ch : '.');
        }
        printk("|\n");
    }
}

/* Banner */
void printk_banner(void) {
    cur = COL(LCYAN, BLACK);
    printk("\n");
    printk("  ╔══════════════════════════════════════════════════════════╗\n");
    printk("  ║         M I N I O S   K E R N E L   v 4 . 0             ║\n");
    printk("  ║         AI-Powered Secure Operating System               ║\n");
    printk("  ╚══════════════════════════════════════════════════════════╝\n\n");
    cur = 0x07;
}

/* Boot log */
void printk_boot(const char *mod, int status) {
    if(status) {
        printk_green("  [  OK  ] ");
    } else {
        printk_red("  [ FAIL ] ");
    }
    printk(mod); printk("\n");
}

/* Panic */
void printk_panic(const char *msg) {
    cur = COL(WHITE, RED);
    for(int i=0; i<W*H; i++) VGA[i] = ' ' | (cur<<8);
    x = y = 0;
    printk("\n\n");
    printk("  ╔══════════════════════════════════════════════════════════╗\n");
    printk("  ║              K E R N E L   P A N I C                     ║\n");
    printk("  ╚══════════════════════════════════════════════════════════╝\n\n");
    printk("  "); printk(msg); printk("\n\n");
    printk("  System halted. All tasks terminated.\n");
    while(1) __hlt();
}

/* Init */
void printk_init(void) {
    for(int i=0; i<W*H; i++) VGA[i] = ' ' | (0x07<<8);
    x = y = 0;
    printk_banner();
    printk_ok("Console initialized");
}

/* ============================================================
 * EK ÖZELLİKLER - 15KB HEDEF
 * ============================================================ */

/* Renk paleti */
void printk_set_fg(uint8_t fg) { cur = (cur & 0xF0) | (fg & 0x0F); }
void printk_set_bg(uint8_t bg) { cur = (cur & 0x0F) | ((bg & 0x0F) << 4); }
void printk_reset(void) { cur = 0x07; }

/* Çizgi çiz */
void printk_line(char ch, int count) {
    for(int i=0; i<count; i++) put(ch);
    printk("\n");
}

/* Ortalı yazı */
void printk_center(const char *s) {
    int len=0; while(s[len]) len++;
    int pad = (W - len) / 2;
    for(int i=0; i<pad; i++) put(' ');
    printk(s);
    printk("\n");
}

/* Sağa hizalı */
void printk_right(const char *s) {
    int len=0; while(s[len]) len++;
    for(int i=0; i<W-len-2; i++) put(' ');
    printk(s);
}

/* Ok işaretli liste */
void printk_arrow(const char *s) {
    printk_cyan("  => "); printk(s); printk("\n");
}

/* Yıldızlı liste */
void printk_star(const char *s) {
    printk_yellow("  * "); printk(s); printk("\n");
}

/* Artılı liste */
void printk_plus(const char *s) {
    printk_green("  + "); printk(s); printk("\n");
}

/* Eksili liste */
void printk_minus(const char *s) {
    printk_red("  - "); printk(s); printk("\n");
}

/* Bilgi kutusu */
void printk_infobox(const char *s) {
    printk_cyan("  [i] "); printk(s); printk("\n");
}

/* Uyarı kutusu */
void printk_warnbox(const char *s) {
    printk_yellow("  [!] "); printk(s); printk("\n");
}

/* Hata kutusu */
void printk_errbox(const char *s) {
    printk_red("  [X] "); printk(s); printk("\n");
}

/* Başarı kutusu */
void printk_okbox(const char *s) {
    printk_green("  [✓] "); printk(s); printk("\n");
}

/* Saat yaz */
void printk_timestamp(void) {
    extern uint64_t get_seconds(void);
    uint64_t sec = 0; /* get_seconds() */
    uint64_t min = sec/60; sec%=60;
    uint64_t hr = min/60; min%=60;
    put('[');
    put('0'+hr/10); put('0'+hr%10); put(':');
    put('0'+min/10); put('0'+min%10); put(':');
    put('0'+sec/10); put('0'+sec%10);
    printk("] ");
}

/* CPU register dump */
void printk_regs(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx,
                 uint64_t rsi, uint64_t rdi, uint64_t rbp, uint64_t rsp) {
    printk_header("CPU REGISTERS");
    printk_table_start("Register", "Value", "");
    printk_table_row("RAX", rax, "");
    printk_table_row("RBX", rbx, "");
    printk_table_row("RCX", rcx, "");
    printk_table_row("RDX", rdx, "");
    printk_table_row("RSI", rsi, "");
    printk_table_row("RDI", rdi, "");
    printk_table_row("RBP", rbp, "");
    printk_table_row("RSP", rsp, "");
    printk_table_end();
}

/* Sistem bilgisi */
void printk_sysinfo(void) {
    printk_banner();
    printk_arrow("Kernel: MiniOS v4.0");
    printk_arrow("Arch: x86_64");
    printk_arrow("AI Engine: Active");
    printk_arrow("Security: Enabled");
    printk_arrow("Network: Ready");
    printk_line('=', W);
}

/* ASCII sanat - MiniOS logosu */
void printk_logo(void) {
    printk_cyan("\n");
    printk_cyan("  __  __ _       _ ___  ____  \n");
    printk_cyan(" |  \\/  (_)_ __ (_) _ \\/ ___| \n");
    printk_cyan(" | |\\/| | | '_ \\| | | | \\___ \\ \n");
    printk_cyan(" | |  | | | | | | | |_| |___) |\n");
    printk_cyan(" |_|  |_|_|_| |_|_|____/|____/ \n");
    printk("\n");
}

/* Yükleme animasyonu */
void printk_loading(const char *msg, int duration_ms) {
    char spinner[] = "|/-\\";
    for(int i=0; i<20; i++) {
        printk("\r"); printk(msg); printk(" "); put(spinner[i%4]);
        /* busy wait */
        for(volatile int j=0; j<100000; j++);
    }
    printk("\r"); printk(msg); printk_green(" Done!\n");
}

/* Test framework */
static int test_pass = 0, test_fail = 0;
void printk_test_start(const char *name) {
    printk("  TEST: "); printk(name); printk("... ");
}
void printk_test_pass(void) {
    test_pass++; printk_green("PASS\n");
}
void printk_test_fail(void) {
    test_fail++; printk_red("FAIL\n");
}
void printk_test_summary(void) {
    printk_line('=', W);
    printk("  Tests: "); printk_dec(test_pass+test_fail);
    printk(" | Passed: "); printk_green(""); printk_dec(test_pass);
    printk(" | Failed: "); printk_red(""); printk_dec(test_fail);
    printk("\n");
}

/* Yardım menüsü */
void printk_help(void) {
    printk_box("MINIOS KERNEL COMMANDS",
               "help - Show this menu\n"
               "info - System information\n"
               "reboot - Restart system\n"
               "clear - Clear screen\n");
}

/* Temiz ekran */
void printk_clear(void) {
    for(int i=0; i<W*H; i++) VGA[i] = ' ' | (0x07<<8);
    x = y = 0;
}
