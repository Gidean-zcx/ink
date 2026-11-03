#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_cache.h"
#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "iss_rt.h"

#define MAX_BLOCKS 64
#define PSRAM_ALIGN 128 /* at least one cache line */

static struct {
    void *p;
    size_t n;
    bool psram;
} B[MAX_BLOCKS];
static int nb;
static const char *region;
static uint32_t t0;

void iss_begin(const char *name) {
    region = name;
    t0 = esp_cpu_get_cycle_count();
}

void iss_end2(uint32_t units, uint32_t aux) {
    uint32_t cycles = esp_cpu_get_cycle_count() - t0;
    printf("HWPROBE region %s units=%" PRIu32 " aux=%" PRIu32 " cycles=%" PRIu32 "\n", region ? region : "?", units,
           aux, cycles);
    region = NULL;
}

void iss_cache(unsigned flags) {
    if (!(flags & (ISS_CACHE_WB | ISS_CACHE_INV_D))) return;
    uint32_t c0 = esp_cpu_get_cycle_count();
    int f = ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA;
    if (flags & ISS_CACHE_INV_D) f |= ESP_CACHE_MSYNC_FLAG_INVALIDATE;
    for (int i = 0; i < nb; i++)
        if (B[i].psram) esp_cache_msync(B[i].p, B[i].n, f);
    /* inside a region the time counts only when the kernel asks for it, as on the ISS */
    if (region && !(flags & ISS_CACHE_CHARGE)) t0 += esp_cpu_get_cycle_count() - c0;
}

uint64_t iss_cycles(void) { return esp_cpu_get_cycle_count(); }

static void *alloc(size_t n, size_t align, bool psram) {
    if (align < 4) align = 4;
    if (psram) {
        if (align < PSRAM_ALIGN) align = PSRAM_ALIGN;
        n = (n + PSRAM_ALIGN - 1) / PSRAM_ALIGN * PSRAM_ALIGN; /* whole lines: msync needs them */
    }
    uint32_t caps = (psram ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL) | MALLOC_CAP_8BIT;
    void *p = nb < MAX_BLOCKS ? heap_caps_aligned_alloc(align, n, caps) : NULL;
    if (!p) {
        printf("HWPROBE error alloc %u bytes in %s failed\n", (unsigned)n, psram ? "PSRAM" : "SRAM");
        abort();
    }
    B[nb].p = p;
    B[nb].n = n;
    B[nb].psram = psram;
    nb++;
    return p;
}

void *iss_sram_alloc(size_t n, size_t align) { return alloc(n, align, false); }
void *iss_psram_alloc(size_t n, size_t align) { return alloc(n, align, true); }

void iss_alloc_reset(void) {
    for (int i = 0; i < nb; i++) heap_caps_free(B[i].p);
    nb = 0;
}
