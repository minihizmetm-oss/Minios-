#include "../include/types.h"

#define KB_DATA_PORT        0x60
#define KB_STATUS_PORT      0x64
#define KB_BUFFER_SIZE      128

static uint8_t kb_buffer[KB_BUFFER_SIZE];
static volatile int kb_head = 0;
static volatile int kb_tail = 0;

static const char scancode_set1[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static bool shift_pressed = false;

void keyboard_init(void) {
    __outb(KB_STATUS_PORT, 0xAE);
    __outb(KB_STATUS_PORT, 0x20);
    while (!(__inb(KB_STATUS_PORT) & 0x01));
    uint8_t cfg = __inb(KB_DATA_PORT);
    cfg |= 0x01;
    cfg &= ~0x40;
    __outb(KB_STATUS_PORT, 0x60);
    __outb(KB_DATA_PORT, cfg);
}

char keyboard_getc(void) {
    while (kb_head == kb_tail) {
        uint8_t status = __inb(KB_STATUS_PORT);
        if (status & 0x01) {
            uint8_t scancode = __inb(KB_DATA_PORT);
            if (!(scancode & 0x80)) {
                if (scancode < sizeof(scancode_set1)) {
                    char c = scancode_set1[scancode];
                    if (c) {
                        if (shift_pressed && c >= 'a' && c <= 'z') c -= 32;
                        int next = (kb_head + 1) % KB_BUFFER_SIZE;
                        if (next != kb_tail) {
                            kb_buffer[kb_head] = c;
                            kb_head = next;
                        }
                    }
                }
                if (scancode == 0x2A || scancode == 0x36) shift_pressed = true;
            } else {
                uint8_t s = scancode & 0x7F;
                if (s == 0x2A || s == 0x36) shift_pressed = false;
            }
        }
    }
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

bool keyboard_has_data(void) {
    return kb_head != kb_tail;
}
