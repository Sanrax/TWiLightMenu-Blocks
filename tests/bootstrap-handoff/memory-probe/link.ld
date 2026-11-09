OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(before_clear)
SECTIONS {
    . = 0x06036200;
    .text : { *(.text*) *(.rodata*) }
    . = ALIGN(4);
    payload_end = .;
    ASSERT(payload_end <= 0x06037000, "Probe overlaps snapshot records")
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
    ASSERT(SIZEOF(.data) == 0 && SIZEOF(.bss) == 0, "Use fixed records only")
    /DISCARD/ : { *(.comment) *(.ARM.attributes) *(.ARM.exidx*) }
}
