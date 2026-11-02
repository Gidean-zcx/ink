/* Shared helpers for the iss calibration kernels.
 *
 * Every kernel follows the same pattern:
 *   1. build its input deterministically (not timed),
 *   2. run once untimed so the I-cache holds the code (steady state),
 *   3. reset the D-cache when the data is meant to stream from PSRAM,
 *   4. measure one region: iss_begin(name) ... iss_end(units),
 *      charging the write-back of dirty PSRAM lines inside the region
 *      (they are written back sooner or later; pessimistic but honest).
 * Region names are "<cost class>@<variant>"; "#cold" marks a run that starts
 * with both caches empty (code fetched from flash). */
#ifndef K_H
#define K_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "iss_rt.h"

static inline uint32_t k_rand(uint32_t *s) { /* xorshift32 */
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

static inline void k_fill_random(uint8_t *p, size_t n, uint32_t seed) {
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)(k_rand(&seed) >> 24);
}

/* Data from data.S (.incbin of kernels/data/). */
extern const uint8_t k_text_zh[], k_text_zh_end[];
extern const uint8_t k_text_zlib[], k_text_zlib_end[];
extern const uint8_t k_json[], k_json_end[];

/* Prevents the compiler from discarding results. */
extern volatile uint32_t k_sink;

void k_mem(void);
void k_gfx(void);
void k_text(void);
void k_crc(void);
void k_json_parse(void);
void k_inflate(void);
void k_tlsf(void);

#endif
