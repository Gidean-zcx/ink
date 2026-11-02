/* Common section layout. The including script defines the region aliases:
 *   CODE   - where .text/.rodata go (FLASH = XIP like an IDF app, SRAM = IRAM)
 * .iram.* code (IRAM_ATTR) always goes to internal SRAM. Data, bss, heap and
 * stack live in internal SRAM; .psram.* (NOLOAD) and the bump allocator in PSRAM. */
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    .text :
    {
        KEEP(*(.text.start))
        *(.text .text.*)
    } > CODE

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata .rodata.* .srodata .srodata.*)
        . = ALIGN(4);
        __init_array_start = .;
        KEEP(*(.init_array .init_array.*))
        __init_array_end = .;
        *(.eh_frame .eh_frame.*)
    } > CODE

    .iram :
    {
        . = ALIGN(4);
        *(.iram .iram.*)
    } > SRAM

    .data :
    {
        . = ALIGN(8);
        *(.data .data.*)
        . = ALIGN(8);
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.*)
    } > SRAM

    .bss (NOLOAD) :
    {
        . = ALIGN(8);
        __bss_start = .;
        *(.sbss .sbss.* .bss .bss.* COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > SRAM

    __heap_start = ALIGN(16);
    __stack_top = ORIGIN(SRAM) + LENGTH(SRAM);
    __heap_end = __stack_top - 0x8000;            /* 32 KiB stack */
    ASSERT(__heap_start <= __heap_end, "internal SRAM overflow")

    .psram (NOLOAD) :
    {
        . = ALIGN(64);
        *(.psram .psram.*)
    } > PSRAM
    __psram_free = ALIGN(64);
    __psram_end = ORIGIN(PSRAM) + LENGTH(PSRAM);

    /DISCARD/ : { *(.comment) *(.note .note.*) *(.riscv.attributes) }
}
