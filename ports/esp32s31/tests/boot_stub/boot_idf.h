#ifndef INK_BOOT_TEST_IDF_H
#define INK_BOOT_TEST_IDF_H
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define RTC_NOINIT_ATTR
#define IRAM_ATTR
#define ESP_OK 0
#define CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ 320
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_DMA 4
#define MALLOC_CAP_SPIRAM 8
#define ESP_ERROR_CHECK(call) assert((call) == ESP_OK)
typedef int esp_err_t;
typedef enum {
    ESP_RST_UNKNOWN, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC,
    ESP_RST_INT_WDT, ESP_RST_TASK_WDT, ESP_RST_WDT, ESP_RST_DEEPSLEEP, ESP_RST_BROWNOUT,
    ESP_RST_SDIO, ESP_RST_USB, ESP_RST_JTAG, ESP_RST_EFUSE, ESP_RST_PWR_GLITCH, ESP_RST_CPU_LOCKUP
} esp_reset_reason_t;
typedef struct { char version[32]; } esp_app_desc_t;
typedef struct { unsigned subtype; size_t size; } esp_partition_t;
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_SUBTYPE_APP_OTA_0 16
#define ESP_PARTITION_SUBTYPE_APP_OTA_1 17
typedef struct { void (*callback)(void *); void *arg; const char *name; } esp_timer_create_args_t;
typedef void *esp_timer_handle_t;
esp_reset_reason_t esp_reset_reason(void);
const esp_app_desc_t *esp_app_get_description(void);
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *esp_partition_find_first(int type, unsigned subtype, const char *label);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p);
void esp_restart(void);
esp_err_t esp_register_shutdown_handler(void (*handler)(void));
esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *out);
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t us);
esp_err_t esp_timer_delete(esp_timer_handle_t timer);
int64_t esp_timer_get_time(void);
size_t heap_caps_get_total_size(unsigned caps);
#endif
