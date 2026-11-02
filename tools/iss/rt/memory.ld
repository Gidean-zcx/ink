/* ESP32-S31 memory map as used by the iss simulator (ESP-IDF soc.h). */
MEMORY
{
    SRAM  (rwx) : ORIGIN = 0x2F000000, LENGTH = 512K   /* internal SRAM, uncached */
    FLASH (rx)  : ORIGIN = 0x40000000, LENGTH = 16M    /* flash XIP via I/D cache */
    PSRAM (rw)  : ORIGIN = 0x50000000, LENGTH = 8M     /* octal PSRAM via D cache */
}
