#include "../include/types.h"

#define VGA_BUFFER          ((volatile uint16_t *)0xFFFFFFFF800B8000)
#define VGA_WIDTH           80
#define VGA_HEIGHT          25
#define VGA_COLOR(fg, bg)   ((bg << 4) | fg)
#define VGA_ENTRY(c, color) ((uint16_t)(c) | ((uint16_t)(color) << 8))

static uint32_t vga_row = 0;
static uint32_t vga_col = 0;
static uint8_t vga_color = 0x07;
static bool panic_mode = false;

void printk(const char *str) {
    while (*str) {
        if (*str == '\n') { vga_col = 0; vga_row++; }
        else {
            VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = VGA_ENTRY(*str, panic_mode ? 0x4F : vga_color);
            vga_col++;
            if (vga_col >= VGA_WIDTH) { vga_col = 0; vga_row++; }
        }
        if (vga_row >= VGA_HEIGHT) { vga_row = 0; vga_col = 0; }
        str++;
    }
}

void printk_info(const char *str) { printk("[INFO] "); printk(str); printk("\n"); }
void printk_warn(const char *str) { printk("[WARN] "); printk(str); printk("\n"); }
void printk_error(const char *str) { printk("[ERROR] "); printk(str); printk("\n"); }

void printk_panic(const char *str) {
    panic_mode = true;
    printk("\n=== KERNEL PANIC ===\n");
    printk(str);
    printk("\nSystem halted.\n");
    __cli();
    while(1) __hlt();
}

void early_printk(const char *str) { printk(str); }
void printk_init(void) { printk("[OK] printk ready\n"); }
