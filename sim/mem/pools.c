#include "pools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tlsf.h"

#define MAX_REGIONS 8

typedef struct {
    const char *name;
    uint8_t *base;
    size_t size;
    tlsf_t tlsf;
    size_t usable;    /* bytes available to allocations after TLSF control overhead */
    size_t used;
    size_t min_free;
    size_t min_largest;
    size_t alloc_count;
    size_t failed;
    pool_t region[MAX_REGIONS];
    int nregions;
} heap_pool_t;

static heap_pool_t P[POOL_COUNT];
static size_t g_always_internal_max = 16;
static size_t g_internal_total_min = (size_t)-1; /* internal + DMA pools: what MALLOC_CAP_INTERNAL can still get */
static pools_warn_fn g_warn;

/* ---- host-side allocation map: attribution per owner tag (not stored in the pools) ---- */
#define MAX_TAGS 128
typedef struct {
    const char *tag;
    size_t cur[POOL_COUNT], peak[POOL_COUNT];
    size_t live;
} tag_stat_t;
static tag_stat_t TAGS[MAX_TAGS];
static int ntags;

typedef struct {
    uintptr_t key; /* 0 = empty, 1 = tombstone */
    uint32_t size;
    uint16_t tag;
    uint8_t pool;
} amap_ent_t;
static amap_ent_t *AM;
static size_t am_cap, am_used;

static int tag_index(const char *tag) {
    if (!tag) tag = "?";
    for (int i = 0; i < ntags; i++)
        if (TAGS[i].tag == tag || !strcmp(TAGS[i].tag, tag)) return i;
    if (ntags >= MAX_TAGS) return MAX_TAGS - 1;
    TAGS[ntags].tag = tag;
    return ntags++;
}

static size_t am_hash(uintptr_t k) { return (size_t)((k >> 4) * 0x9E3779B97F4A7C15ull); }

static void am_put(uintptr_t k, uint32_t size, int tag, int pool);
static void am_grow(void) {
    amap_ent_t *old = AM;
    size_t oldcap = am_cap;
    am_cap = am_cap ? am_cap * 2 : 4096;
    AM = calloc(am_cap, sizeof *AM);
    if (!AM) abort();
    am_used = 0;
    for (size_t i = 0; i < oldcap; i++)
        if (old[i].key > 1) am_put(old[i].key, old[i].size, old[i].tag, old[i].pool);
    free(old);
}

static void am_put(uintptr_t k, uint32_t size, int tag, int pool) {
    if ((am_used + 1) * 2 > am_cap) am_grow();
    size_t i = am_hash(k) & (am_cap - 1);
    while (AM[i].key > 1) i = (i + 1) & (am_cap - 1);
    AM[i].key = k;
    AM[i].size = size;
    AM[i].tag = (uint16_t)tag;
    AM[i].pool = (uint8_t)pool;
    am_used++;
}

static amap_ent_t *am_find(uintptr_t k) {
    if (!am_cap) return NULL;
    size_t i = am_hash(k) & (am_cap - 1);
    while (AM[i].key) {
        if (AM[i].key == k) return &AM[i];
        i = (i + 1) & (am_cap - 1);
    }
    return NULL;
}

static void attribute(void *ptr, size_t block, int pool, const char *owner) {
    int t = tag_index(owner);
    TAGS[t].cur[pool] += block;
    TAGS[t].live++;
    if (TAGS[t].cur[pool] > TAGS[t].peak[pool]) TAGS[t].peak[pool] = TAGS[t].cur[pool];
    am_put((uintptr_t)ptr, (uint32_t)block, t, pool);
}

static void unattribute(void *ptr) {
    amap_ent_t *e = am_find((uintptr_t)ptr);
    if (!e) return;
    TAGS[e->tag].cur[e->pool] -= e->size;
    TAGS[e->tag].live--;
    e->key = 1; /* tombstone */
}

int pools_tag_count(void) { return ntags; }
void pools_tag_info(int i, const char **tag, size_t cur[POOL_COUNT], size_t peak[POOL_COUNT], size_t *live) {
    *tag = TAGS[i].tag;
    for (int p = 0; p < POOL_COUNT; p++) {
        cur[p] = TAGS[i].cur[p];
        peak[p] = TAGS[i].peak[p];
    }
    *live = TAGS[i].live;
}

static const char *NAMES[POOL_COUNT] = {"internal", "dma", "external"};

static void walk_largest(void *ptr, size_t size, int used, void *user) {
    (void)ptr;
    size_t *max = user;
    if (!used && size > *max) *max = size;
}

static size_t largest_free(heap_pool_t *p) {
    if (!p->tlsf) return 0;
    size_t max = 0;
    for (int r = 0; r < p->nregions; r++) tlsf_walk_pool(p->region[r], walk_largest, &max);
    return max;
}

static void walk_free(void *ptr, size_t size, int used, void *user) {
    (void)ptr;
    if (!used) *(size_t *)user += size;
}

/* A pool may consist of several separate regions (the device's internal heap is split by the
 * memory map and static data): a block never spans regions, so the largest possible allocation
 * is bounded by the largest region. */
static void pool_init(heap_pool_t *p, const char *name, const size_t *regions, int nregions, size_t overhead) {
    memset(p, 0, sizeof *p);
    p->name = name;
    size_t ctl = tlsf_size(), total = ctl;
    size_t rsz[MAX_REGIONS];
    if (nregions < 1) nregions = 1;
    if (nregions > MAX_REGIONS) nregions = MAX_REGIONS;
    for (int r = 0; r < nregions; r++) {
        size_t want = regions[r] > overhead ? regions[r] - overhead : 0; /* the device allocator's own control data */
        rsz[r] = (want + tlsf_pool_overhead() + 15) & ~(size_t)15;
        if (rsz[r] < 1024) rsz[r] = 1024;
        total += rsz[r];
    }
    p->size = total;
    p->base = aligned_alloc(16, (total + 15) & ~(size_t)15);
    if (!p->base) {
        fprintf(stderr, "pools: cannot reserve %zu bytes for %s\n", total, name);
        abort();
    }
    p->tlsf = tlsf_create(p->base);
    uint8_t *at = p->base + ((ctl + 15) & ~(size_t)15);
    for (int r = 0; r < nregions; r++) {
        p->region[r] = tlsf_add_pool(p->tlsf, at, rsz[r]);
        at += rsz[r];
    }
    p->nregions = nregions;
    size_t freeb = 0;
    for (int r = 0; r < nregions; r++) tlsf_walk_pool(p->region[r], walk_free, &freeb);
    p->usable = freeb;
    p->min_free = p->usable;
    p->min_largest = largest_free(p);
}

void pools_init(const size_t *internal_regions, int n_internal, size_t dma_bytes, size_t external_bytes,
                size_t region_overhead) {
    pool_init(&P[POOL_INTERNAL], NAMES[0], internal_regions, n_internal, region_overhead);
    pool_init(&P[POOL_DMA], NAMES[1], &dma_bytes, 1, region_overhead);
    pool_init(&P[POOL_EXTERNAL], NAMES[2], &external_bytes, 1, region_overhead);
}

void pools_set_policy(size_t always_internal_max) { g_always_internal_max = always_internal_max; }
void pools_set_warn(pools_warn_fn fn) { g_warn = fn; }
const char *pools_name(pool_id_t id) { return NAMES[id]; }

static heap_pool_t *owner_of(void *ptr) {
    for (int i = 0; i < POOL_COUNT; i++)
        if ((uint8_t *)ptr >= P[i].base && (uint8_t *)ptr < P[i].base + P[i].size) return &P[i];
    return NULL;
}

static void track_after_change(heap_pool_t *p) {
    size_t free_now = p->usable > p->used ? p->usable - p->used : 0;
    if (free_now < p->min_free) p->min_free = free_now;
    size_t fi = P[POOL_INTERNAL].usable > P[POOL_INTERNAL].used ? P[POOL_INTERNAL].usable - P[POOL_INTERNAL].used : 0;
    size_t fd = P[POOL_DMA].usable > P[POOL_DMA].used ? P[POOL_DMA].usable - P[POOL_DMA].used : 0;
    if (fi + fd < g_internal_total_min) g_internal_total_min = fi + fd;
}

size_t pools_internal_total_min_free(void) { return g_internal_total_min == (size_t)-1 ? 0 : g_internal_total_min; }

static size_t g_internal_largest_min = (size_t)-1; /* min over time of the largest block internal-only allocations can get */

static void track_largest(heap_pool_t *p) {
    if (p == &P[POOL_EXTERNAL]) return; /* MiBs: sampled periodically instead (walk cost) */
    size_t l = largest_free(p);
    if (l < p->min_largest) p->min_largest = l;
    size_t li = p == &P[POOL_INTERNAL] ? l : largest_free(&P[POOL_INTERNAL]);
    size_t ld = p == &P[POOL_DMA] ? l : largest_free(&P[POOL_DMA]);
    size_t best = li > ld ? li : ld;
    if (best < g_internal_largest_min) g_internal_largest_min = best;
}

size_t pools_internal_total_min_largest(void) { return g_internal_largest_min == (size_t)-1 ? 0 : g_internal_largest_min; }

static void *try_alloc(heap_pool_t *p, size_t size) {
    void *ptr = tlsf_malloc(p->tlsf, size ? size : 1);
    if (ptr) {
        p->used += tlsf_block_size(ptr);
        p->alloc_count++;
        track_after_change(p);
        track_largest(p);
    }
    return ptr;
}

void *pools_alloc(size_t size, uint32_t caps, const char *owner) {
    heap_pool_t *order[4] = {0};
    int n = 0;
    /* ESP-IDF 6.1 on the S31: regular internal RAM carries DMA|INTERNAL|DEFAULT at priority 0; the
     * PSRAM reserve (SPIRAM_MALLOC_RESERVE_INTERNAL) carries {0, DMA|INTERNAL, DEFAULT}: it is tried
     * after regular RAM for DMA and internal requests, and last of all for default ones. */
    if (caps & (OSAL_MEM_DMA | OSAL_MEM_INTERNAL)) {
        order[n++] = &P[POOL_INTERNAL];
        order[n++] = &P[POOL_DMA];
    } else if (caps & OSAL_MEM_EXTERNAL) {
        order[n++] = &P[POOL_EXTERNAL];
    } else if (size <= g_always_internal_max) {
        order[n++] = &P[POOL_INTERNAL];
        order[n++] = &P[POOL_DMA];
        order[n++] = &P[POOL_EXTERNAL];
    } else {
        order[n++] = &P[POOL_EXTERNAL];
        order[n++] = &P[POOL_INTERNAL];
        order[n++] = &P[POOL_DMA];
    }
    for (int i = 0; i < n; i++) {
        void *ptr = try_alloc(order[i], size);
        if (ptr) {
            attribute(ptr, tlsf_block_size(ptr), (int)(order[i] - P), owner);
            return ptr;
        }
    }
    order[0]->failed++;
    if (g_warn) {
        char msg[200];
        snprintf(msg, sizeof msg, "allocation of %zu bytes failed (caps=0x%x, owner=%s, %s free=%zu largest=%zu)",
                 size, caps, owner ? owner : "?", order[0]->name,
                 order[0]->usable > order[0]->used ? order[0]->usable - order[0]->used : 0, largest_free(order[0]));
        g_warn(msg);
    }
    return NULL;
}

void pools_free(void *ptr) {
    if (!ptr) return;
    heap_pool_t *p = owner_of(ptr);
    if (!p) {
        fprintf(stderr, "pools: free of foreign pointer %p\n", ptr);
        abort();
    }
    p->used -= tlsf_block_size(ptr);
    p->alloc_count--;
    unattribute(ptr);
    tlsf_free(p->tlsf, ptr);
}

size_t pools_block_size(void *ptr) { return ptr ? tlsf_block_size(ptr) : 0; }

void *pools_realloc(void *ptr, size_t size, uint32_t caps, const char *owner) {
    if (!ptr) return pools_alloc(size, caps, owner);
    if (size == 0) {
        pools_free(ptr);
        return NULL;
    }
    size_t old = tlsf_block_size(ptr);
    if (old >= size) return ptr;
    heap_pool_t *p = owner_of(ptr);
    if (!p) {
        fprintf(stderr, "pools: realloc of foreign pointer %p\n", ptr);
        abort();
    }
    uint32_t want = caps;
    if (!want) want = (p == &P[POOL_EXTERNAL]) ? OSAL_MEM_EXTERNAL : OSAL_MEM_INTERNAL;
    void *n = pools_alloc(size, want, owner);
    if (!n) return NULL;
    memcpy(n, ptr, old);
    pools_free(ptr);
    return n;
}

void pools_sample(void) {
    for (int i = 0; i < POOL_COUNT; i++) {
        size_t l = largest_free(&P[i]);
        if (l < P[i].min_largest) P[i].min_largest = l;
        track_after_change(&P[i]);
    }
}

size_t pools_min_largest(pool_id_t id) { return P[id].min_largest; }
bool pools_owns(const void *ptr) { return owner_of((void *)ptr) != NULL; }
bool pools_is_external(const void *ptr) { return owner_of((void *)ptr) == &P[POOL_EXTERNAL]; }
int pools_pool_of(const void *ptr) {
    heap_pool_t *p = owner_of((void *)ptr);
    return p ? (int)(p - P) : -1;
}

void pools_stats(pool_id_t id, osal_mem_stats_t *out) {
    heap_pool_t *p = &P[id];
    out->total = p->usable;
    out->free = p->usable > p->used ? p->usable - p->used : 0;
    out->min_free = p->min_free;
    out->largest_free = largest_free(p);
    out->alloc_count = p->alloc_count;
    out->failed_allocs = p->failed;
}
