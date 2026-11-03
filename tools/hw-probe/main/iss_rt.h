/* The iss target API (tools/iss/rt/iss_rt.h) on the real ESP32-S31 under ESP-IDF, so the
 * calibration kernels compile unchanged and their regions are timed with the CPU cycle counter
 * instead of the instruction-set simulator. Each region prints one "HWPROBE region" line.
 *
 * Differences from the simulator: ISS_CACHE_INV_I is a no-op (code stays in the I-cache, so
 * "#cold" regions run warm), and only PSRAM blocks from iss_psram_alloc() are written back or
 * invalidated (internal SRAM is not cached; flash rodata is left alone). */
#ifndef ISS_RT_H
#define ISS_RT_H

#include <stddef.h>
#include <stdint.h>

#include "esp_attr.h"

#define ISS_CACHE_WB 1u     /* write back dirty D-cache lines */
#define ISS_CACHE_INV_D 2u  /* invalidate the D-cache */
#define ISS_CACHE_INV_I 4u  /* invalidate the I-cache (no-op here) */
#define ISS_CACHE_CHARGE 8u /* charge the write-back time to the open region */

void iss_begin(const char *name);
/* units = work done in the region; aux = optional second count (0 = none) */
void iss_end2(uint32_t units, uint32_t aux);
static inline void iss_end(uint32_t units) { iss_end2(units, 0); }
void iss_cache(unsigned flags);
static inline void iss_cache_cold(void) { iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D | ISS_CACHE_INV_I); }
static inline void iss_cache_drain(void) { iss_cache(ISS_CACHE_WB | ISS_CACHE_CHARGE); }
uint64_t iss_cycles(void);

void *iss_sram_alloc(size_t n, size_t align);
void *iss_psram_alloc(size_t n, size_t align);
void iss_alloc_reset(void);

#define ISS_IRAM IRAM_ATTR __attribute__((noinline))

#endif
