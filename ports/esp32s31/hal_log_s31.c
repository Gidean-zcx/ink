#include <stdio.h>
#include <string.h>
#include "ink/hal_log.h"
#include "ink/hal_device.h"
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_core_dump.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static RTC_NOINIT_ATTR hal_log_tail_t tail;
hal_log_tail_t *hal_log_tail(void) { return &tail; }
static const esp_partition_t *region(bool core) {
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, core ? "coredump" : "font_data");
    return p && p->address == (core ? 0xfe4000u : 0xae4000u) && p->size >= (core ? 65536u : 0x500000u) ? p : NULL;
}
bool hal_log_read(bool core, uint32_t off, void *out, size_t n) {
    const esp_partition_t *p = region(core);
    uint32_t size = core ? 65536u : INK_LOG_BYTES;
    return p && off <= size && n <= size - off && esp_partition_read(p, (core ? 0 : 0x480000u) + off, out, n) == ESP_OK;
}
bool hal_log_erase(uint32_t off) {
    const esp_partition_t *p = region(false);
    return p && off % INK_LOG_SECTOR == 0 && off < INK_LOG_BYTES && esp_partition_erase_range(p, 0x480000u + off, INK_LOG_SECTOR) == ESP_OK;
}
bool hal_log_program(uint32_t off, const void *data, size_t n) {
    const esp_partition_t *p = region(false);
    return p && off <= INK_LOG_BYTES && n <= INK_LOG_BYTES - off && esp_partition_write(p, 0x480000u + off, data, n) == ESP_OK;
}
int hal_log_format(char *out, size_t n, const char *fmt, va_list a) { return vsnprintf(out, n, fmt, a); }
/* hal_device_reset_reason lives in hal_device_s31.c: "panic", "task-watchdog", ... (the log's crash check looks for those words). */
void hal_log_boot_info(char *out, size_t n) {
    const esp_app_desc_t *a = esp_app_get_description();
    const esp_partition_t *p = esp_ota_get_running_partition();
    snprintf(out, n, "version=%.32s commit=%.40s partition=%.16s reset_raw=%d", a->version, INK_BUILD_COMMIT, p ? p->label : "unknown", (int)esp_reset_reason());
    if (esp_core_dump_image_check() == ESP_OK) {
        esp_core_dump_summary_t *s = heap_caps_malloc(sizeof *s, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s && esp_core_dump_get_summary(s) == ESP_OK) {
            size_t used = strlen(out);
            snprintf(out + used, n - used, " coredump task=%.16s pc=%08lx ra=%08lx backtrace=host-ELF", s->exc_task, (unsigned long)s->exc_pc, (unsigned long)s->ex_info.ra);
        }
        heap_caps_free(s);
    }
}
void hal_log_tasks(char *out, size_t n) {
    unsigned count = uxTaskGetNumberOfTasks() + 8;
    TaskStatus_t *t = heap_caps_malloc(count * sizeof *t, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    unsigned got = t ? uxTaskGetSystemState(t, count, NULL) : 0;
    int pos = snprintf(out, n, "tasks=%u sampled=%u stack_low=", count - 8, got);
    for (unsigned i = 0; i < got && i < 5 && pos >= 0 && (size_t)pos < n; i++) {
        unsigned best = i;
        for (unsigned j = i + 1; j < got; j++)
            if (t[j].usStackHighWaterMark < t[best].usStackHighWaterMark) best = j;
        TaskStatus_t tmp = t[i]; t[i] = t[best]; t[best] = tmp;
        pos += snprintf(out + pos, n - (size_t)pos, " %.16s:%lu", t[i].pcTaskName, (unsigned long)t[i].usStackHighWaterMark);
    }
    heap_caps_free(t);
}

/* Every log line also goes to the USB console, live (system/services/log/log.c calls this; the line ends in a newline). */
void hal_log_echo(const char *line) { esp_log_write(ESP_LOG_INFO, "ink", "%s", line); }
