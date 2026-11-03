/* Compile the exact audited implementation in this translation unit to compare
 * its published stats with its own TLSF walk. No product source is changed. */
#include "../../../../sim/mem/pools.c"
void vtk_host_call(void (*fn)(void *), void *arg) { fn(arg); }
int main(void) {
    size_t region = 65536;
    pools_init(&region, 1, 0, 65536, 0);
    void *both = pools_alloc(64, OSAL_MEM_EXTERNAL | OSAL_MEM_DMA, "probe");
    printf("external_plus_dma: returned_pool=%d (0=internal,2=external)\n", pools_pool_of(both));
    pools_free(both);
    void *ptrs[100];
    for (int i = 0; i < 100; i++) ptrs[i] = pools_alloc(64, OSAL_MEM_INTERNAL, "probe");
    osal_mem_stats_t s;
    pools_stats(POOL_INTERNAL, &s);
    size_t actual = 0;
    tlsf_walk_pool(P[POOL_INTERNAL].region[0], walk_free, &actual);
    printf("100 allocations: reported_free=%zu tlsf_free=%zu overstatement=%zu largest=%zu\n",
           s.free, actual, s.free - actual, s.largest_free);
    pools_stats(POOL_DMA, &s);
    printf("configured_dma=0 reported_total=%zu\n", s.total);
    for (int i = 0; i < 100; i++) pools_free(ptrs[i]);
    for (int i = 0; i < 100000; i++) {
        void *p = pools_alloc(64, OSAL_MEM_INTERNAL, "probe");
        pools_free(p);
    }
    pools_stats(POOL_INTERNAL, &s);
    printf("allocation_map_after_100000_alloc_free: capacity=%zu entries_counter=%zu live=%zu host_bytes=%zu failed_allocs=%zu\n",
           am_cap, am_used, s.alloc_count, am_cap * sizeof *AM, s.failed_allocs);
    return 0;
}
