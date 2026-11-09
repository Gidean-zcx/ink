/* ESP-IDF v6.1 FreeRTOS port. No simulator dependencies. */
#include "ink/osal.h"
#include "osal_ticks.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/idf_additions.h"

struct osal_task { TaskHandle_t handle; osal_task_fn fn; void *arg; };
struct osal_timer { esp_timer_handle_t handle; osal_timer_fn fn; void *arg; uint64_t period; bool first, deleting; };
static portMUX_TYPE wall_lock = portMUX_INITIALIZER_UNLOCKED;
static int64_t wall_base;
static bool wall_set;
static size_t failed_allocs[2];
#define TASK_TLS (configNUM_THREAD_LOCAL_STORAGE_POINTERS - 1)

uint64_t osal_now_us(void) { return (uint64_t)esp_timer_get_time(); }
int64_t osal_wall_us(void) {
    portENTER_CRITICAL(&wall_lock);
    int64_t value = wall_set ? wall_base + (int64_t)osal_now_us() : 0;
    portEXIT_CRITICAL(&wall_lock);
    return value;
}
void osal_set_wall_us(int64_t us) {
    portENTER_CRITICAL(&wall_lock);
    wall_base = us - (int64_t)osal_now_us();
    wall_set = us != 0;
    portEXIT_CRITICAL(&wall_lock);
}
static uint32_t mem_caps(uint32_t caps) {
    if (caps & ~(OSAL_MEM_INTERNAL | OSAL_MEM_EXTERNAL | OSAL_MEM_DMA)) return 0;
    if ((caps & OSAL_MEM_INTERNAL) && (caps & OSAL_MEM_EXTERNAL)) return 0;
    uint32_t value = MALLOC_CAP_8BIT;
    value |= caps & OSAL_MEM_EXTERNAL ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL;
    if (caps & OSAL_MEM_DMA) value |= MALLOC_CAP_DMA;
    return value;
}
void *osal_realloc(void *p, size_t size, uint32_t caps) {
    if (!caps) {
        if (size > 16) {
            void *q = heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (q) return q;
        }
    }
    uint32_t value = mem_caps(caps);
    void *q = value ? heap_caps_realloc(p, size, value) : NULL;
    if (!q && size) __atomic_add_fetch(&failed_allocs[!!(caps & OSAL_MEM_EXTERNAL)], 1, __ATOMIC_RELAXED);
    return q;
}
void *osal_malloc(size_t size, uint32_t caps) { return osal_realloc(NULL, size, caps); }
void *osal_calloc(size_t n, size_t size, uint32_t caps) {
    if (size && n > SIZE_MAX / size) return NULL;
    void *p = osal_malloc(n * size, caps);
    if (p) memset(p, 0, n * size);
    return p;
}
void osal_free(void *p) { heap_caps_free(p); }
void osal_mem_stats(uint32_t caps, osal_mem_stats_t *out) {
    multi_heap_info_t info;
    heap_caps_get_info(&info, mem_caps(caps));
    *out = (osal_mem_stats_t){.total = info.total_free_bytes + info.total_allocated_bytes,
        .free = info.total_free_bytes, .min_free = info.minimum_free_bytes,
        .largest_free = info.largest_free_block, .alloc_count = info.allocated_blocks,
        .failed_allocs = __atomic_load_n(&failed_allocs[!!(caps & OSAL_MEM_EXTERNAL)], __ATOMIC_RELAXED)};
}
const char *osal_mem_tag(const char *tag) { (void)tag; return NULL; }
static TickType_t wait_ticks(int64_t us) {
    /* Read both clocks without migration or a tick interrupt between them. */
    portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&lock);
    uint64_t now = osal_now_us();
    TickType_t tick = xTaskGetTickCount();
    portEXIT_CRITICAL(&lock);
    return ink_s31_wait_ticks(now, tick, us, configTICK_RATE_HZ, portMAX_DELAY);
}
osal_task_t *osal_task_self(void) { return pvTaskGetThreadLocalStoragePointer(NULL, TASK_TLS); }
const char *osal_task_name(const osal_task_t *t) { return pcTaskGetName(t ? t->handle : NULL); }
void osal_task_exit(void) {
    osal_task_t *t = osal_task_self();
    vTaskSetThreadLocalStoragePointer(NULL, TASK_TLS, NULL);
    osal_free(t);
    vTaskDeleteWithCaps(NULL);
}
static void task_entry(void *arg) {
    osal_task_t *t = arg;
    t->handle = xTaskGetCurrentTaskHandle();
    vTaskSetThreadLocalStoragePointer(NULL, TASK_TLS, t);
    t->fn(t->arg);
    osal_task_exit();
}
osal_err_t osal_task_create(const osal_task_cfg_t *cfg, osal_task_t **out) {
    if (out) *out = NULL;
    if (!cfg || !cfg->fn || cfg->stack_bytes < 512 || cfg->priority < 0 ||
        cfg->priority > OSAL_PRIO_MAX || cfg->core < -1 || cfg->core >= configNUMBER_OF_CORES ||
        (cfg->flags & ~OSAL_TASK_STACK_EXTERNAL)) return OSAL_INVALID;
    osal_task_t *t = osal_calloc(1, sizeof *t, OSAL_MEM_INTERNAL);
    if (!t) return OSAL_NO_MEM;
    t->fn = cfg->fn;
    t->arg = cfg->arg;
    if (out) *out = t;
    uint32_t caps = MALLOC_CAP_8BIT | (cfg->flags & OSAL_TASK_STACK_EXTERNAL ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL);
    if (xTaskCreatePinnedToCoreWithCaps(task_entry, cfg->name ? cfg->name : "ink", cfg->stack_bytes,
            t, cfg->priority, NULL, cfg->core < 0 ? tskNO_AFFINITY : cfg->core, caps) == pdPASS) return OSAL_OK;
    if (out) *out = NULL;
    osal_free(t);
    return OSAL_NO_MEM;
}
void osal_task_delay_us(uint64_t us) {
    if (!us) { taskYIELD(); return; }
    uint64_t start = osal_now_us();
    for (;;) {
        uint64_t elapsed = osal_now_us() - start;
        if (elapsed >= us) return;
        uint64_t left = us - elapsed;
        vTaskDelay(wait_ticks(left > INT64_MAX ? INT64_MAX : (int64_t)left));
    }
}
void osal_task_yield(void) { taskYIELD(); }
void osal_preempt_point(void) { }
void osal_cost_cycles(uint64_t cycles) { (void)cycles; }
void osal_cost_units(const char *name, uint64_t units) { (void)name; (void)units; }

osal_queue_t *osal_queue_create(size_t size, size_t n) { return size && n && n <= SIZE_MAX / size ? (osal_queue_t *)xQueueCreate(n, size) : NULL; }
void osal_queue_delete(osal_queue_t *q) { if (q) vQueueDelete((QueueHandle_t)q); }
osal_err_t osal_queue_send(osal_queue_t *q, const void *p, int64_t us) {
    if (!q || !p) return OSAL_INVALID;
    return xQueueSend((QueueHandle_t)q, p, wait_ticks(us)) == pdPASS ? OSAL_OK : (us ? OSAL_TIMEOUT : OSAL_FULL);
}
osal_err_t osal_queue_send_front(osal_queue_t *q, const void *p, int64_t us) {
    if (!q || !p) return OSAL_INVALID;
    return xQueueSendToFront((QueueHandle_t)q, p, wait_ticks(us)) == pdPASS ? OSAL_OK : (us ? OSAL_TIMEOUT : OSAL_FULL);
}
osal_err_t osal_queue_recv(osal_queue_t *q, void *p, int64_t us) {
    if (!q || !p) return OSAL_INVALID;
    return xQueueReceive((QueueHandle_t)q, p, wait_ticks(us)) == pdPASS ? OSAL_OK : (us ? OSAL_TIMEOUT : OSAL_EMPTY);
}
size_t osal_queue_count(osal_queue_t *q) { return q ? uxQueueMessagesWaiting((QueueHandle_t)q) : 0; }
osal_mutex_t *osal_mutex_create(void) { return (osal_mutex_t *)xSemaphoreCreateMutex(); }
void osal_mutex_delete(osal_mutex_t *m) { if (m) vSemaphoreDelete((SemaphoreHandle_t)m); }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t us) {
    if (!m) return OSAL_INVALID;
    return xSemaphoreTake((SemaphoreHandle_t)m, wait_ticks(us)) == pdTRUE ? OSAL_OK : OSAL_TIMEOUT;
}
void osal_mutex_unlock(osal_mutex_t *m) { if (m) xSemaphoreGive((SemaphoreHandle_t)m); }
osal_sem_t *osal_sem_create(uint32_t initial, uint32_t max) {
    return max && initial <= max ? (osal_sem_t *)xSemaphoreCreateCounting(max, initial) : NULL;
}
void osal_sem_delete(osal_sem_t *s) { if (s) vSemaphoreDelete((SemaphoreHandle_t)s); }
osal_err_t osal_sem_take(osal_sem_t *s, int64_t us) { return osal_mutex_lock((osal_mutex_t *)s, us); }
void osal_sem_give(osal_sem_t *s) { if (s) xSemaphoreGive((SemaphoreHandle_t)s); }

static void timer_callback(void *arg) {
    osal_timer_t *t = arg;
    if (t->first && t->period) {
        t->first = false;
        ESP_ERROR_CHECK(esp_timer_start_periodic(t->handle, t->period));
    }
    t->fn(t->arg); /* May delete t. */
}
osal_timer_t *osal_timer_create(const char *name, osal_timer_fn fn, void *arg) {
    if (!fn) return NULL;
    osal_timer_t *t = osal_calloc(1, sizeof *t, OSAL_MEM_INTERNAL);
    if (!t) return NULL;
    t->fn = fn;
    t->arg = arg;
    esp_timer_create_args_t cfg = {.callback = timer_callback, .arg = t, .name = name, .dispatch_method = ESP_TIMER_TASK};
    if (esp_timer_create(&cfg, &t->handle) == ESP_OK) return t;
    osal_free(t);
    return NULL;
}
osal_err_t osal_timer_stop(osal_timer_t *t) {
    if (!t) return OSAL_INVALID;
    return esp_timer_stop_blocking(t->handle, portMAX_DELAY) == ESP_OK ? OSAL_OK : OSAL_INVALID;
}
osal_err_t osal_timer_start(osal_timer_t *t, uint64_t delay, uint64_t period) {
    if (!t || __atomic_load_n(&t->deleting, __ATOMIC_ACQUIRE) || delay > INT64_MAX || period > INT64_MAX) return OSAL_INVALID;
    if (osal_timer_stop(t) != OSAL_OK) return OSAL_INVALID;
    t->period = period;
    t->first = true;
    return esp_timer_start_once(t->handle, delay ? delay : 1) == ESP_OK ? OSAL_OK : OSAL_INVALID;
}
void osal_timer_delete(osal_timer_t *t) {
    if (!t) return;
    __atomic_store_n(&t->deleting, true, __ATOMIC_RELEASE);
    ESP_ERROR_CHECK(esp_timer_stop_blocking(t->handle, portMAX_DELAY));
    (void)esp_timer_stop(t->handle); /* A callback finishing above may have armed the periodic phase. */
    ESP_ERROR_CHECK(esp_timer_delete(t->handle));
    osal_free(t);
}
void osal_panic(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
    abort();
}
