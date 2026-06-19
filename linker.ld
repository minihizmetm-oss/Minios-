ENTRY(_start)
SECTIONS
{
    . = 1M;
    .note.PVH : { *(.note.PVH) }
    .multiboot2 : { *(.multiboot2) }
    .text : { *(.text) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) }
}
