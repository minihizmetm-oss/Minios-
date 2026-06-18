# MiniOS Kernel v4.0 - Makefile (Termux/Clang uyumlu)

CC = clang
LD = ld.lld
AS = nasm

CFLAGS = -ffreestanding -O2 -Wall -Wextra \
         -fno-exceptions -fno-rtti -fno-stack-protector \
         -mno-red-zone -mcmodel=large -target x86_64-none-elf \
         -nostdlib -I./include -DKERNEL_VERSION=\"4.0\"

LDFLAGS = -T arch/x86_64/linker.ld -nostdlib
ASFLAGS = -f elf64

SRCS = kernel_main.c \
       lib/printk.c \
       arch/x86_64/gdt.c \
       arch/x86_64/idt.c \
       mm/pmm.c \
       mm/vmm.c \
       drivers/timer.c \
       drivers/keyboard.c \
       sched/scheduler.c \
       ai/ai_engine.c \
       security/security.c

ASMS = arch/x86_64/boot.asm \
       arch/x86_64/isr.asm \
       arch/x86_64/context_switch.asm

OBJS = $(SRCS:.c=.o) $(ASMS:.asm=.o)

all: minios.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

minios.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

run: minios.bin
	qemu-system-x86_64 -kernel minios.bin -m 3G -serial stdio

debug: minios.bin
	qemu-system-x86_64 -kernel minios.bin -m 3G -serial stdio -d int,cpu_reset -no-reboot -no-shutdown

clean:
	rm -f $(OBJS) minios.bin

.PHONY: all run debug clean

run2: minios.bin
	qemu-system-x86_64 -kernel minios.bin -m 256M -nographic -no-reboot
