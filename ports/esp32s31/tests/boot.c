/* RTC contents and IDF services are fake; execute the actual boot guard and app_main. */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/hal_ota.h"
#include "ink/hal_display.h"
#include "ink/osal.h"
#ifndef INK_BOOT_SOURCE
#define INK_BOOT_SOURCE "../s31_boot.c"
#endif
#include INK_BOOT_SOURCE

void app_main(void);
static esp_reset_reason_t reason;
static const esp_app_desc_t app = {.version = "g12345678-261110123456"};
static esp_partition_t running = {.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1, .size = 5 * 1024 * 1024};
static const esp_partition_t factory = {.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0};
static bool missing, reject, logged, allow_restart;
static unsigned writes, restarts, started, step;
static int64_t now, deadline;
static esp_timer_create_args_t timer;
static void (*shutdown_handler)(void);
static jmp_buf reboot;

esp_reset_reason_t esp_reset_reason(void) { return reason; }
const esp_app_desc_t *esp_app_get_description(void) { return &app; }
const esp_partition_t *esp_ota_get_running_partition(void) { return &running; }
const esp_partition_t *esp_partition_find_first(int type, unsigned subtype, const char *label) {
    assert(type == ESP_PARTITION_TYPE_APP && subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 && label == NULL);
    return missing ? NULL : &factory;
}
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p) {
    assert(p == &factory && logged && step == 0);
    writes++;
    return reject ? -1 : ESP_OK;
}
void esp_restart(void) { assert(allow_restart && step == 0); restarts++; longjmp(reboot, 1); }
void ink_s31_display_shutdown(void) { }
esp_err_t esp_register_shutdown_handler(void (*handler)(void)) { shutdown_handler = handler; return ESP_OK; }
esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *out) {
    timer = *args;
    *out = &timer;
    return ESP_OK;
}
esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t us) {
    assert(t == &timer && us > 0);
    deadline = now + (int64_t)us;
    return ESP_OK;
}
esp_err_t esp_timer_delete(esp_timer_handle_t t) { assert(t == &timer); deadline = 0; return ESP_OK; }
int64_t esp_timer_get_time(void) { return now; }
size_t heap_caps_get_total_size(unsigned caps) { return caps * 1024; }
void hal_log_write(const char *line) {
    if (strstr(line, "selecting factory ota_0")) logged = true;
    if (strstr(line, "ink: E-Ink4 Plus")) assert(strstr(line, app.version) && strstr(line, hal_device_reset_reason()));
}
void ink_s31_osal_selftest(void) { assert(step++ == 0 && shutdown_handler == ink_s31_display_shutdown); }
void ink_s31_board_init(void) { assert(step++ == 1); }
void hal_display_get_info(hal_panel_info_t *out) { assert(step++ == 2); *out = (hal_panel_info_t){.width = 1216, .height = 684}; }
void ink_main(void) { assert(step++ == 3); started++; }
void osal_panic(const char *format, ...) { (void)format; abort(); }
static void boot(esp_reset_reason_t why) {
    reason = why;
    now = 0;
    step = 0;
    logged = false;
    app_main();
    assert(step == 4 && deadline == 60000000);
}
static void healthy(void) {
    now = 59999999;
    assert(ink_boot.armed && deadline > now);
    now = 60000000;
    timer.callback(timer.arg);
    assert(ink_boot.failures == 0 && ink_boot.armed == 0 && deadline == 0);
}
int main(void) {
    boot(ESP_RST_POWERON);
    assert(ink_boot.failures == 0);
    boot(ESP_RST_PANIC);
    assert(ink_boot.failures == 1 && writes == 0);
    boot(ESP_RST_TASK_WDT);
    assert(ink_boot.failures == 2 && writes == 0);
    allow_restart = true;
    if (setjmp(reboot) == 0) { boot(ESP_RST_INT_WDT); assert(false); }
    assert(writes == 1 && restarts == 1 && started == 3);
    allow_restart = false;

    const esp_reset_reason_t fresh[] = {ESP_RST_POWERON, ESP_RST_SW, ESP_RST_BROWNOUT, ESP_RST_DEEPSLEEP};
    for (unsigned i = 0; i < sizeof fresh / sizeof fresh[0]; i++) {
        ink_boot_save(2, true);
        boot(fresh[i]);
        assert(ink_boot.failures == 0 && writes == 1);
    }
    ink_boot_save(2, true);
    ink_boot.check ^= 1;
    boot(ESP_RST_PANIC);
    assert(ink_boot.failures == 0 && writes == 1);
    ink_boot_save(2, true);
    ink_boot.magic ^= 1;
    boot(ESP_RST_WDT);
    assert(ink_boot.failures == 0);
    ink_boot_save(100, true);
    boot(ESP_RST_WDT);
    assert(ink_boot.failures == 0);
    boot(ESP_RST_CPU_LOCKUP);
    assert(ink_boot.failures == 1);
    healthy();
    boot(ESP_RST_PANIC);
    assert(ink_boot.failures == 0 && writes == 1);

    missing = true;
    ink_boot_save(2, true);
    boot(ESP_RST_PANIC);
    assert(writes == 1);
    missing = false;
    running.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
    ink_boot_save(2, true);
    boot(ESP_RST_PANIC);
    assert(writes == 1);
    running.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
    reject = true;
    ink_boot_save(2, true);
    boot(ESP_RST_PANIC);
    assert(writes == 2 && restarts == 1);

    for (int i = ESP_RST_UNKNOWN; i <= ESP_RST_CPU_LOCKUP; i++) {
        reason = i;
        assert(hal_device_reset_reason()[0]);
        if (i != ESP_RST_UNKNOWN) assert(strcmp(hal_device_reset_reason(), "unknown") != 0);
    }
    reason = ESP_RST_TASK_WDT;
    assert(strcmp(hal_device_reset_reason(), "task-watchdog") == 0);
    hal_device_info_t info;
    hal_device_get_info(&info);
    assert(!strcmp(info.profile_id, "metalio-e-ink4-plus") && strstr(info.display_name, "ESP32-S31"));
    assert(info.cpu_cores == 2 && info.cpu_mhz == 320 && info.internal_heap_bytes && info.external_heap_bytes);
    hal_ota_info_t ota;
    hal_ota_info(&ota);
    assert(!strcmp(ota.version, app.version) && ota.running == 1 && ota.slot_size == running.size);
    puts("s31 boot: full entry, RTC checksum, three early resets, 60s clear, ota_0 selection and device info passed");
    return 0;
}
