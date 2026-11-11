/* Bounds recorder for the actual S31 HAL. It cannot access hardware. */
#include <assert.h>
#include "s31_stub/idf.h"
#include "ink/hal_log.h"
#include "ink/hal_device.h"
static esp_partition_t font = {.address = 0xae4000, .size = 0x500000, .label = "font_data"};
static const esp_partition_t core = {.address = 0xfe4000, .size = 65536, .label = "coredump"};
static unsigned writes, erases, reads;
/* The vendor's flash drain: every flash operation runs with the I2C1 lock held (s31_flash_begin). */
static unsigned drains, held;
static bool draining;
bool s31_flash_begin(void) { assert(!draining); draining = true; drains++; return true; }
void s31_flash_end(bool taken) { assert(taken && draining); draining = false; }
static esp_reset_reason_t reset = ESP_RST_POWERON;
const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *label) {
    assert(type == ESP_PARTITION_TYPE_DATA && subtype == ESP_PARTITION_SUBTYPE_ANY);
    if (!strcmp(label, "font_data")) return &font;
    assert(!strcmp(label, "coredump")); return &core;
}
int esp_partition_read(const esp_partition_t *p, uint32_t off, void *out, size_t n) {
    assert(off <= p->size && n <= p->size - off);
    assert(p == &core || off >= 0x480000); memset(out, 255, n); reads++; held += draining; return ESP_OK;
}
int esp_partition_write(const esp_partition_t *p, uint32_t off, const void *data, size_t n) {
    (void)data; assert(p == &font && off >= 0x480000 && off + n <= 0x500000); writes++; held += draining; return ESP_OK;
}
int esp_partition_erase_range(const esp_partition_t *p, uint32_t off, size_t n) {
    assert(p == &font && off >= 0x480000 && off + n <= 0x500000 && off % 4096 == 0 && n == 4096); erases++; held += draining; return ESP_OK;
}
const esp_partition_t *esp_ota_get_running_partition(void) { return &font; }
const esp_app_desc_t *esp_app_get_description(void) { static const esp_app_desc_t a = {.version = "fake-version"}; return &a; }
esp_reset_reason_t esp_reset_reason(void) { return reset; }
int esp_core_dump_image_check(void) { return ESP_OK; }
int esp_core_dump_get_summary(esp_core_dump_summary_t *s) { *s = (esp_core_dump_summary_t){.exc_task = "fake-crash", .exc_pc = 0x1234, .ex_info.ra = 0x5678}; return ESP_OK; }
unsigned uxTaskGetNumberOfTasks(void) { return 1; }
unsigned uxTaskGetSystemState(TaskStatus_t *s, unsigned n, void *time) { (void)n; (void)time; *s = (TaskStatus_t){.pcTaskName = "fake-task", .usStackHighWaterMark = 512}; return 1; }
int main(void) {
    char text[256];
    assert(hal_log_tail() && sizeof *hal_log_tail() >= 16384);
    assert(hal_log_read(false, 0, text, sizeof text));
    assert(hal_log_read(true, 65536 - sizeof text, text, sizeof text));
    assert(!hal_log_read(true, 65536, text, 1));
    assert(!hal_log_read(false, INK_LOG_BYTES, text, 1));
    assert(!hal_log_read(false, UINT32_MAX, text, sizeof text));
    assert(hal_log_erase(0) && hal_log_erase(INK_LOG_BYTES - 4096));
    assert(!hal_log_erase(1) && !hal_log_erase(INK_LOG_BYTES));
    assert(hal_log_program(INK_LOG_BYTES - 1, text, 1));
    assert(!hal_log_program(INK_LOG_BYTES - 1, text, 2));
    font.address++; assert(!hal_log_erase(0) && !hal_log_program(0, text, 1)); font.address--;
    font.size--; assert(!hal_log_read(false, 0, text, 1)); font.size++;
    assert(writes == 1 && erases == 2 && reads == 2);
    assert(held == writes + erases + reads && drains == held && !draining); /* no flash call outside a drain */
    hal_log_boot_info(text, sizeof text); assert(strstr(text, "fake-version") && strstr(text, "fake-crash") && strstr(text, "00001234"));
    hal_log_tasks(text, sizeof text); assert(strstr(text, "fake-task:512"));
    return 0;
}
