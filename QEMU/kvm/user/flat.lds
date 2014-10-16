OUTPUT_FORMAT(binary)

SECTIONS
{
    . = 1M;
    stext = .;
    .text : { *(.init) *(.text) *(.text.*) }
    . = ALIGN(4K);
    .data : { *(.data) }
    . = ALIGN(16);
    .rodata : { *(.rodata) }
    . = ALIGN(16);
    .bss : { *(.bss) }
    . = ALIGN(4K);
    edata = .;
}

