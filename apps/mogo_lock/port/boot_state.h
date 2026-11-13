/* Mogo's boot-resume guard: no asset is ever quarantined here. */
#pragma once
#include <stdbool.h>
static inline bool pico_boot_asset_allowed(const char *path) { (void)path; return true; }
