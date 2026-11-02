/* Everything in internal SRAM, like Espressif's CoreMark component
 * (linker fragment "* (noflash)": text -> IRAM, rodata -> DRAM). */
INCLUDE memory.ld
REGION_ALIAS("CODE", SRAM);
INCLUDE sections.ld
