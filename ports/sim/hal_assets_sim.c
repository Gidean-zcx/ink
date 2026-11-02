/*
 * Assets in the simulator: host files from the --assets directories, loaded
 * once and treated as flash-mapped (no heap cost). The total must fit the
 * profile's assets partition.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_assets.h"
#include "sim.h"

#define MAX_ASSETS 64

static struct {
    char name[96];
    void *data;
    size_t size;
    int maps;
} AS[MAX_ASSETS];
static int nAS;
static size_t total_bytes;

const void *hal_asset_map(const char *name, size_t *size) {
    for (int i = 0; i < nAS; i++)
        if (!strcmp(AS[i].name, name)) {
            AS[i].maps++;
            if (size) *size = AS[i].size;
            return AS[i].data;
        }
    if (nAS >= MAX_ASSETS || strstr(name, "..")) return NULL;
    for (int d = 0; d < g_sim.nasset_dirs; d++) {
        char path[600];
        snprintf(path, sizeof path, "%s/%s", g_sim.asset_dirs[d], name);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        void *buf = malloc((size_t)n ? (size_t)n : 1);
        if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) {
            fclose(f);
            free(buf);
            return NULL;
        }
        fclose(f);
        snprintf(AS[nAS].name, sizeof AS[nAS].name, "%s", name);
        AS[nAS].data = buf;
        AS[nAS].size = (size_t)n;
        AS[nAS].maps = 1;
        nAS++;
        total_bytes += (size_t)n;
        sim_metric_set("assets.mapped_bytes", (double)total_bytes);
        if (g_profile.assets_partition_bytes && total_bytes > g_profile.assets_partition_bytes)
            sim_warn("assets: %zu KiB mapped exceeds the %zu KiB assets partition of this device", total_bytes / 1024,
                     g_profile.assets_partition_bytes / 1024);
        sim_charge_ns_always(SIM_US(200)); /* partition table lookup + MMU mapping */
        if (size) *size = (size_t)n;
        return buf;
    }
    sim_warn("asset '%s' not found in the --assets directories", name);
    return NULL;
}

/* ---- D-cache model for mapped flash: set-associative LRU over 64-byte lines ----
 * Lines are identified by asset and offset (the partition is 64 KiB aligned on the device).
 * sim_assets_cache_pollute() (called when a refresh streams the PSRAM framebuffers through
 * the same cache) empties it. */
#define DC_WAYS 2
static struct {
    uint64_t *tag; /* sets x ways, 0 = empty */
    uint8_t *lru;  /* per set: index of the least recently used way */
    uint32_t sets;
    uint64_t hits, misses;
} DC;

static void dc_init(void) {
    if (DC.tag || !g_profile.dcache_bytes) return;
    DC.sets = g_profile.dcache_bytes / g_profile.dcache_line / DC_WAYS;
    if (!DC.sets) return;
    DC.tag = calloc((size_t)DC.sets * DC_WAYS, sizeof *DC.tag);
    DC.lru = calloc(DC.sets, 1);
    if (!DC.tag || !DC.lru) abort();
}

void sim_assets_cache_pollute(void) {
    if (DC.tag) memset(DC.tag, 0, (size_t)DC.sets * DC_WAYS * sizeof *DC.tag);
}

void hal_asset_access(const void *p, size_t len) {
    if (!len || !g_profile.dcache_bytes || vtk_in_isr() || !vtk_self()) return;
    dc_init();
    if (!DC.tag) return;
    const uint8_t *b = p;
    int ai = -1;
    for (int i = 0; i < nAS; i++)
        if (b >= (const uint8_t *)AS[i].data && b < (const uint8_t *)AS[i].data + AS[i].size) ai = i;
    if (ai < 0) return;
    uint64_t off = (uint64_t)(b - (const uint8_t *)AS[ai].data);
    uint64_t first = off / g_profile.dcache_line, last = (off + len - 1) / g_profile.dcache_line;
    unsigned misses = 0;
    for (uint64_t l = first; l <= last; l++) {
        uint64_t tag = ((uint64_t)(ai + 1) << 40) | (l + 1);
        uint32_t set = (uint32_t)((l ^ ((uint64_t)ai * 2654435761u)) % DC.sets);
        uint64_t *w = &DC.tag[(size_t)set * DC_WAYS];
        if (w[0] == tag) {
            DC.lru[set] = 1;
            DC.hits++;
        } else if (w[1] == tag) {
            DC.lru[set] = 0;
            DC.hits++;
        } else {
            int victim = DC.lru[set];
            w[victim] = tag;
            DC.lru[set] = (uint8_t)(victim ^ 1);
            misses++;
        }
    }
    if (misses) {
        DC.misses += misses;
        sim_charge_ns_always(sim_cycles_ns((double)misses * profile_cost("flash.line_fill")));
    }
}

void sim_assets_report(void) {
    sim_metric_set("assets.dcache_hits", (double)DC.hits);
    sim_metric_set("assets.dcache_misses", (double)DC.misses);
}

void hal_asset_unmap(const void *data) {
    for (int i = 0; i < nAS; i++)
        if (AS[i].data == data && AS[i].maps > 0) AS[i].maps--;
}
