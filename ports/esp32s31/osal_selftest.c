/* Boot self-test: compiled here; execution requires a separately authorized device run. */
#include "ink/osal.h"
#include "ink/hal_device.h"
#include "esp_memory_utils.h"
#include <string.h>
#define CHECK(x) do { if (!(x)) osal_panic("OSAL selftest: %s", #x); } while (0)
static void signal_timer(void *arg) { osal_sem_give(arg); }
static void worker(void *arg) {
    CHECK(osal_task_self() != NULL);
    CHECK(strcmp(osal_task_name(osal_task_self()), "osal-test") == 0);
    unsigned char local;
    CHECK(esp_ptr_external_ram(&local));
    osal_sem_give(arg);
}
static osal_timer_t *self_timer;
static void self_delete(void *arg) { osal_timer_delete(self_timer); osal_sem_give(arg); }
void ink_s31_osal_selftest(void) {
    void *p = osal_calloc(4, 32, OSAL_MEM_EXTERNAL);
    CHECK(p && esp_ptr_external_ram(p) && ((unsigned char *)p)[127] == 0);
    p = osal_realloc(p, 256, OSAL_MEM_INTERNAL);
    CHECK(p && esp_ptr_internal(p));
    osal_free(p);
    CHECK(!osal_malloc(32, OSAL_MEM_INTERNAL | OSAL_MEM_EXTERNAL));
    CHECK(!osal_calloc(SIZE_MAX, 2, OSAL_MEM_DEFAULT));
    osal_mem_stats_t stats;
    osal_mem_stats(OSAL_MEM_INTERNAL, &stats);
    CHECK(stats.total >= stats.free && stats.free > 0);
    CHECK(osal_wall_us() == 0);
    osal_set_wall_us(1000000);
    CHECK(osal_wall_us() >= 1000000);
    osal_set_wall_us(0);
    osal_queue_t *q = osal_queue_create(sizeof(int), 2);
    int a = 1, b = 2, out = 0;
    CHECK(q && osal_queue_recv(q, &out, 0) == OSAL_EMPTY);
    CHECK(osal_queue_send(q, &a, 0) == OSAL_OK);
    CHECK(osal_queue_send_front(q, &b, 0) == OSAL_OK);
    CHECK(osal_queue_count(q) == 2 && osal_queue_send(q, &a, 0) == OSAL_FULL);
    CHECK(osal_queue_recv(q, &out, 0) == OSAL_OK && out == b);
    CHECK(osal_queue_recv(q, &out, 0) == OSAL_OK && out == a);
    uint64_t start = osal_now_us();
    CHECK(osal_queue_recv(q, &out, 1001) == OSAL_TIMEOUT);
    CHECK(osal_now_us() - start >= 1001);
    osal_queue_delete(q);
    osal_mutex_t *m = osal_mutex_create();
    CHECK(m && osal_mutex_lock(m, 0) == OSAL_OK);
    osal_mutex_unlock(m);
    osal_mutex_delete(m);
    osal_sem_t *s = osal_sem_create(0, 1);
    CHECK(s && osal_sem_take(s, 0) == OSAL_TIMEOUT);
    osal_task_cfg_t cfg = {.name = "osal-test", .fn = worker, .arg = s, .stack_bytes = 2048,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK);
    CHECK(osal_sem_take(s, 1000000) == OSAL_OK);
    osal_timer_t *t = osal_timer_create("osal-test", signal_timer, s);
    start = osal_now_us();
    CHECK(t && osal_timer_start(t, 20001, 30001) == OSAL_OK);
    CHECK(osal_sem_take(s, 1000000) == OSAL_OK && osal_now_us() - start >= 20000);
    CHECK(osal_sem_take(s, 1000000) == OSAL_OK);
    CHECK(osal_timer_stop(t) == OSAL_OK);
    CHECK(osal_sem_take(s, 50000) == OSAL_TIMEOUT);
    CHECK(osal_timer_start(t, 1000, 0) == OSAL_OK);
    osal_timer_delete(t);
    CHECK(osal_sem_take(s, 50000) == OSAL_TIMEOUT);
    self_timer = osal_timer_create("self-delete", self_delete, s);
    CHECK(self_timer && osal_timer_start(self_timer, 1000, 0) == OSAL_OK);
    CHECK(osal_sem_take(s, 1000000) == OSAL_OK);
    osal_sem_delete(s);
    hal_log_write("OSAL 自检完成");
}
