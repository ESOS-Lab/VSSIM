OUTPUT_FORMAT(binary)

SECTIONS
{
    . = 0;
    stext = .;
    .text : { *(.init) *(.text) }
    . = ALIGN(4K);
    .data : { *(.data) *(.rodata*) }
    . = ALIGN(16);
    .bss : { *(.bss) }
    . = 0xfff0;
    .boot : { *(.boot) }
    edata = .;
}

