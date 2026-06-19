CC = clang
LD = ld.lld
AS = nasm
CFLAGS = -ffreestanding -O2 -Wall -Wextra -m32 -target i386-none-elf -nostdlib -I./include
LDFLAGS = -T arch/x86_64/linker.ld -nostdlib -m elf_i386
ASFLAGS = -f elf32

SRCS = drivers/disk.c fs/elf.c drivers/usb.c drivers/acpi.c drivers/audio.c fs/turkfs.c drivers/pci.c drivers/e1000.c lib/math.c kernel_main.c lib/printk.c arch/x86_64/gdt.c arch/x86_64/idt.c mm/pmm.c mm/vmm.c drivers/timer.c drivers/keyboard.c sched/scheduler.c ai/ai_engine.c security/security.c
ASMS = arch/x86_64/boot.asm arch/x86_64/isr.asm arch/x86_64/context_switch.asm
OBJS = $(SRCS:.c=.o) $(ASMS:.asm=.o)

all: minios.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

minios.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(OBJS) minios.bin
