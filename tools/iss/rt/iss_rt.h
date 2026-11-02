/* Target-side API for programs run on the iss simulator.
 *
 *   iss_begin("glyph.blit"); ...work...; iss_end(units);
 * records one measured region (cycles, instructions, cache statistics) in the
 * simulator's JSON report. The calls themselves are free (not timed). */
#ifndef ISS_RT_H
#define ISS_RT_H

#include <stddef.h>
#include <stdint.h>

#define ISS_CACHE_WB 1u     /* write back dirty D-cache lines */
#define ISS_CACHE_INV_D 2u  /* invalidate the D-cache */
#define ISS_CACHE_INV_I 4u  /* invalidate the I-cache */
#define ISS_CACHE_CHARGE 8u /* charge the write-back time to the program */

static inline long iss_ecall(long n, long arg) {
    register long a0 __asm__("a0") = arg;
    register long a7 __asm__("a7") = n;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

static inline long iss_ecall2(long n, long arg0, long arg1) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a7 __asm__("a7") = n;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
    return a0;
}

static inline void iss_begin(const char *name) { iss_ecall(0x100, (long)name); }
/* units = work done in the region; aux = optional second count (0 = none) */
static inline void iss_end2(uint32_t units, uint32_t aux) { iss_ecall2(0x101, (long)units, (long)aux); }
static inline void iss_end(uint32_t units) { iss_end2(units, 0); }
static inline void iss_cache(unsigned flags) { iss_ecall(0x102, (long)flags); }

/* Cold start: write back + invalidate both caches without charging time. */
static inline void iss_cache_cold(void) { iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D | ISS_CACHE_INV_I); }
/* Charge the write-back of every dirty line (end of a streaming store). */
static inline void iss_cache_drain(void) { iss_cache(ISS_CACHE_WB | ISS_CACHE_CHARGE); }

static inline uint64_t iss_cycles(void) {
    uint32_t hi, lo, hi2;
    do {
        __asm__ volatile("rdcycleh %0" : "=r"(hi));
        __asm__ volatile("rdcycle %0" : "=r"(lo));
        __asm__ volatile("rdcycleh %0" : "=r"(hi2));
    } while (hi != hi2);
    return (uint64_t)hi << 32 | lo;
}

/* Bump allocators: internal SRAM pool and PSRAM. Never freed except by reset. */
void *iss_sram_alloc(size_t n, size_t align);
void *iss_psram_alloc(size_t n, size_t align);
void iss_alloc_reset(void);

#define ISS_IRAM __attribute__((section(".iram.text"), noinline))

#endif
