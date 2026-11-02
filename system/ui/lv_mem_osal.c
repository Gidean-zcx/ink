/* LVGL allocator glue: LVGL objects and buffers come from the device heaps. */
#include "ink/osal.h"
#include "lvgl.h"

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes) {
    (void)mem;
    (void)bytes;
    return NULL;
}
void lv_mem_remove_pool(lv_mem_pool_t pool) { (void)pool; }

void *lv_malloc_core(size_t size) {
    const char *prev = osal_mem_tag("lvgl");
    void *p = osal_malloc(size, OSAL_MEM_DEFAULT);
    osal_mem_tag(prev);
    return p;
}

void *lv_realloc_core(void *p, size_t new_size) {
    const char *prev = osal_mem_tag("lvgl");
    void *n = osal_realloc(p, new_size, OSAL_MEM_DEFAULT);
    osal_mem_tag(prev);
    return n;
}

void lv_free_core(void *p) { osal_free(p); }

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p) {
    osal_mem_stats_t st;
    osal_mem_stats(OSAL_MEM_EXTERNAL, &st);
    lv_memzero(mon_p, sizeof(*mon_p));
    mon_p->total_size = st.total;
    mon_p->free_size = st.free;
    mon_p->free_biggest_size = st.largest_free;
    mon_p->used_pct = st.total ? (uint8_t)(100 - st.free * 100 / st.total) : 0;
}

lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
