#ifndef INK_LOG_TEST_IDF_H
#define INK_LOG_TEST_IDF_H
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define RTC_NOINIT_ATTR
#define ESP_OK 0
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_ANY 255
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
typedef enum { ESP_RST_POWERON, ESP_RST_PANIC, ESP_RST_INT_WDT, ESP_RST_TASK_WDT, ESP_RST_WDT, ESP_RST_BROWNOUT } esp_reset_reason_t;
typedef struct { uint32_t address, size; char label[17]; } esp_partition_t;
typedef struct { char version[33]; } esp_app_desc_t;
typedef struct { char exc_task[16]; uint32_t exc_pc; struct { uint32_t ra; } ex_info; } esp_core_dump_summary_t;
typedef struct { const char *pcTaskName; uint32_t usStackHighWaterMark; } TaskStatus_t;
const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *label);
int esp_partition_read(const esp_partition_t *p, uint32_t off, void *out, size_t n);
int esp_partition_write(const esp_partition_t *p, uint32_t off, const void *data, size_t n);
int esp_partition_erase_range(const esp_partition_t *p, uint32_t off, size_t n);
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_app_desc_t *esp_app_get_description(void);
esp_reset_reason_t esp_reset_reason(void);
int esp_core_dump_image_check(void);
int esp_core_dump_get_summary(esp_core_dump_summary_t *s);
unsigned uxTaskGetNumberOfTasks(void);
unsigned uxTaskGetSystemState(TaskStatus_t *s, unsigned n, void *time);
static inline void *heap_caps_malloc(size_t n, int caps) { (void)caps; return malloc(n); }
static inline void heap_caps_free(void *p) { free(p); }
#endif
