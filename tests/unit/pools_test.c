/*
 * Device memory pools (simulator/memory/pools.c), checked against TLSF itself: what the pools
 * report as free must be what TLSF has free, also after fragmentation (audit AUD2-010), and the
 * allocation map must not grow with the number of allocations made (AUD2-011). The pool code is
 * included so the test can walk the same TLSF instance.
 */
#include "../../simulator/memory/pools.c"

void vtk_host_call(void (*fn)(void *), void *arg) { fn(arg); }

static int failures;

static void check(int ok, const char *what, long long a, long long b) {
    printf("%s %s (%lld, %lld)\n", ok ? "ok  " : "FAIL", what, a, b);
    if (!ok) failures++;
}

static size_t tlsf_free_internal(void) {
    size_t actual = 0;
    for (int r = 0; r < P[POOL_INTERNAL].nregions; r++)
        if (P[POOL_INTERNAL].region[r]) tlsf_walk_pool(P[POOL_INTERNAL].region[r], walk_free, &actual);
    return actual;
}

int main(void) {
    size_t region = 65536;
    pools_init(&region, 1, 0, 65536, 0);
    osal_mem_stats_t s;
    pools_stats(POOL_DMA, &s);
    check(s.total == 0, "a pool configured with 0 bytes has none", (long long)s.total, 0);

    void *ptrs[100];
    for (int i = 0; i < 100; i++) ptrs[i] = pools_alloc(64, OSAL_MEM_INTERNAL, "test");
    pools_stats(POOL_INTERNAL, &s);
    check(s.free == tlsf_free_internal(), "free after 100 allocations is TLSF's", (long long)s.free, (long long)tlsf_free_internal());

    for (int i = 0; i < 100; i += 2) {
        pools_free(ptrs[i]);
        ptrs[i] = NULL;
    }
    pools_stats(POOL_INTERNAL, &s);
    check(s.free == tlsf_free_internal(), "free after every other one was freed (fragmented) is TLSF's", (long long)s.free,
          (long long)tlsf_free_internal());
    check(s.min_free <= s.free, "the low-water mark is not above the free space", (long long)s.min_free, (long long)s.free);

    for (int i = 1; i < 100; i += 2) pools_free(ptrs[i]);
    pools_stats(POOL_INTERNAL, &s);
    check(s.free == s.total, "everything freed: all of it free again", (long long)s.free, (long long)s.total);

    for (int i = 0; i < 100000; i++) pools_free(pools_alloc(64, OSAL_MEM_INTERNAL, "test"));
    check(am_cap <= 4096, "the allocation map stays small after 100000 allocate/free", (long long)am_cap, 4096);

    void *both = pools_alloc(64, OSAL_MEM_EXTERNAL | OSAL_MEM_DMA, "test");
    check(pools_pool_of(both) == POOL_EXTERNAL, "DMA-capable PSRAM comes from the external pool", pools_pool_of(both), POOL_EXTERNAL);
    pools_free(both);
    return failures ? 1 : 0;
}
