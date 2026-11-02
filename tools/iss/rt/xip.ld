/* Application layout: code and constants execute from flash (XIP). */
INCLUDE memory.ld
REGION_ALIAS("CODE", FLASH);
INCLUDE sections.ld
