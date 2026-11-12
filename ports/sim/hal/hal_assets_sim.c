/* The simulator reads the device packs; compressed mappings use reclaimable PSRAM. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_assets.h"
#include "reader_miniz/miniz.h"
#include "sim.h"

#define MAX_ASSETS 256 /* the device looks each name up in the packs, with no table: 64 ran out on a tour of every page */

static struct {
    char name[96];
    void *data;
    size_t size;
    bool compressed;
} AS[MAX_ASSETS];
static int nAS;
static size_t total_bytes;

static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int load(const char *name) {
    static const char *const packs[] = {"ink-fonts.bin", "ink-embedded.bin"};
    for (int d = 0; d < g_sim.nasset_dirs; d++) for (unsigned k = 0; k < 2; k++) {
        char path[600];
        snprintf(path, sizeof path, "%s/%s", g_sim.asset_dirs[d], packs[k]);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        uint8_t h[12], e[104];
        if (fread(h, 1, sizeof h, f) != sizeof h || memcmp(h, "INKA", 4) ||
            word(h + 8) < 12 || word(h + 4) > (word(h + 8) - 12) / 104) { fclose(f); return -1; }
        uint32_t count = word(h + 4), bytes = word(h + 8);
        for (uint32_t i = 0; i < count; i++) {
            if (fread(e, 1, sizeof e, f) != sizeof e || !memchr(e, 0, 96)) { fclose(f); return -1; }
            if (strcmp((const char *)e, name)) continue;
            uint32_t off = word(e + 96), n = word(e + 100) & 0x7fffffffu;
            if (off < 12 + count * 104 || off > bytes || n > bytes - off || fseek(f, off, SEEK_SET)) {
                fclose(f); return -1;
            }
            void *buf = sim_host_malloc(n ? n : 1);
            if (!buf || fread(buf, 1, n, f) != n) { fclose(f); sim_host_free(buf); return -1; }
            fclose(f);
            sim_copy_name(AS[nAS].name, sizeof AS[nAS].name, name);
            AS[nAS].data = buf;
            AS[nAS].size = n;
            AS[nAS].compressed = (word(e + 100) & 0x80000000u) != 0;
            return 1;
        }
        fclose(f);
    }
    return 0;
}
static const void *mapped(int i, size_t *size) {
    const uint8_t *p = AS[i].data;
    size_t n = AS[i].size;
    if (!AS[i].compressed) { if (size) *size = n; return p; }
    if (n < 4 || !word(p) || word(p) > 5 * 1024 * 1024) return NULL;
    hal_asset_access(p, n);
    size_t want = word(p), in = n - 4, out = want;
    const char *prev = osal_mem_tag("assets");
    uint8_t *buf = osal_malloc(want, OSAL_MEM_EXTERNAL);
    tinfl_decompressor *d = osal_malloc(sizeof *d, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    tinfl_status status = TINFL_STATUS_FAILED;
    if (d && buf) {
        tinfl_init(d);
        status = tinfl_decompress(d, p + 4, &in, buf, buf, &out,
            TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    }
    osal_free(d);
    if (status != TINFL_STATUS_DONE || out != want || in != n - 4) { osal_free(buf); return NULL; }
    if (size) *size = want;
    return buf;
}

typedef struct {
    const char *name;
    int result;
} load_t;

static void load_on_host(void *p) {
    load_t *l = p;
    l->result = load(l->name);
}

const void *hal_asset_map(const char *name, size_t *size) {
    if (size) *size = 0;
    if (!name) return NULL;
    for (int i = 0; i < nAS; i++)
        if (!strcmp(AS[i].name, name)) {
            return mapped(i, size);
        }
    if (nAS >= MAX_ASSETS || strstr(name, "..")) return NULL;
    load_t l = {name, 0};
    vtk_host_call(load_on_host, &l); /* reading the host file is not the caller's stack */
    if (l.result < 0) return NULL;
    if (l.result > 0) {
        size_t n = AS[nAS].size;
        int i = nAS++;
        total_bytes += n;
        sim_metric_set("assets.mapped_bytes", (double)total_bytes);
        if (g_profile.assets_partition_bytes && total_bytes > g_profile.assets_partition_bytes)
            sim_warn("assets: %zu KiB mapped exceeds the %zu KiB assets partition of this device", total_bytes / 1024,
                     g_profile.assets_partition_bytes / 1024);
        sim_charge_ns_always(SIM_US(200)); /* partition table lookup + MMU mapping */
        return mapped(i, size);
    }
    sim_warn("asset '%s' not found in the device asset packs", name);
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
    DC.tag = sim_host_calloc((size_t)DC.sets * DC_WAYS, sizeof *DC.tag);
    DC.lru = sim_host_calloc(DC.sets, 1);
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
    if (data && pools_owns(data)) osal_free((void *)data);
}
