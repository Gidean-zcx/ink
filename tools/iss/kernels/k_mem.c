/* memcpy / memset between internal SRAM and PSRAM (newlib's memcpy/memset,
 * as linked by the toolchain). Per byte. PSRAM sides stream 256 KiB (4x the
 * D-cache) from a cold cache; dirty lines are written back inside the region.
 * The calls go through volatile pointers: with -O2 -mtune=esp-base GCC would
 * otherwise inline constant-size copies as word loops, while real call sites
 * mostly pass run-time sizes and reach the library (on the device: ROM) code. */
#include "k.h"

#define SB (32u * 1024u)  /* SRAM block */
#define PB (256u * 1024u) /* PSRAM stream */

static void *(*volatile p_memcpy)(void *, const void *, size_t) = memcpy;
static void *(*volatile p_memset)(void *, int, size_t) = memset;
#define memcpy(d, s, n) p_memcpy((d), (s), (n))
#define memset(d, c, n) p_memset((d), (c), (n))

void k_mem(void) {
    iss_alloc_reset();
    uint8_t *s1 = iss_sram_alloc(SB, 64), *s2 = iss_sram_alloc(SB, 64);
    uint8_t *p1 = iss_psram_alloc(PB, 64), *p2 = iss_psram_alloc(PB, 64);
    k_fill_random(s1, SB, 1);
    k_fill_random(p1, PB, 2);

    /* warm the I-cache with every routine used below */
    memcpy(s2, s1, 4096);
    memset(s2, 0, 4096);

    iss_begin("sram.copy_byte");
    for (int r = 0; r < 4; r++) memcpy(s2, s1, SB);
    iss_end(4 * SB);

    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    iss_begin("psram_to_internal.copy_byte");
    for (uint32_t off = 0; off < PB; off += SB) memcpy(s2, p1 + off, SB);
    iss_end(PB);

    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    iss_begin("internal_to_psram.copy_byte");
    for (uint32_t off = 0; off < PB; off += SB) memcpy(p2 + off, s1, SB);
    iss_cache_drain();
    iss_end(PB);

    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    iss_begin("psram.copy_byte");
    memcpy(p2, p1, PB);
    iss_cache_drain();
    iss_end(PB);

    iss_begin("sram.set_byte");
    for (int r = 0; r < 4; r++) memset(s2, 0x5a, SB);
    iss_end(4 * SB);

    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    iss_begin("psram.set_byte");
    memset(p2, 0xa5, PB);
    iss_cache_drain();
    iss_end(PB);

    /* small copies (4 KiB) whose source and destination are already cached */
    memcpy(p2, p1, 4096);
    iss_begin("psram.copy_byte@cached4k");
    for (int r = 0; r < 8; r++) memcpy(p2, p1, 4096);
    iss_end(8 * 4096);

    k_sink = s2[SB - 1] ^ p2[PB - 1];
}
