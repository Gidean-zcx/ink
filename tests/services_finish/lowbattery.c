#include "product.h"
#ifndef INK_TEST_POWER_SOURCE
#define INK_TEST_POWER_SOURCE "../../system/services/power/power_service.c"
#endif
#include INK_TEST_POWER_SOURCE
static const ink_db_def_t DEF = {.name="finish.lowbattery", .scope=INK_DB_DEVICE, .schema=1};
static char reason[32];
static bool saved;
static void (*original_picture)(const char *why);
extern void (*sim_power_before_shutdown)(void);
static void picture(const char *why) {
    snprintf(reason, sizeof reason, "%s", why);
    original_picture(why);
}
static void before_shutdown(void) {
    on_ui(read_screen);
    if (saved) {
        CHECK(!strcmp(reason, "battery") && strstr(g_snap, "东西都存好了"), "AUD18-403: saved state is truthful (%s)", g_snap);
    } else {
        CHECK(!strcmp(reason, "battery_unsaved") && sim_metric_value("power.shutdown_unsaved") == 1, "AUD18-403: timeout reaches the presenter as unsaved (%s)", reason);
        CHECK(strstr(g_snap, "没保存") && !strstr(g_snap, "都存好了"), "AUD18-403: failed save is not presented as success (%s)", g_snap);
    }
    ink_probe_count("test.finished");
}
static void run(void *arg) {
    (void)arg; delay_ms(2200);
    saved = getenv("INK_FINISH_SAVED") != NULL;
    CHECK(ink_db_register(&DEF) == 0, "collection");
    ink_db_t *db = ink_db_open(&DEF, NULL);
    CHECK(db != NULL, "open");
    if (!saved) ink_storage_hold_writes(true);
    ink_db_write_t w = {.data="SYNTHETIC", .len=9, .flags=INK_DB_LAZY};
    CHECK(ink_db_put(db, &w, NULL) == INK_DB_OK, "edit accepted");
    original_picture = PS.off_picture;
    ink_power_set_off_picture(picture);
    sim_power_before_shutdown = before_shutdown;
    hw(HW_BATTERY, 2, 0);
    ink_power_lock();
    delay_ms(15000);
    CHECK(false, "low battery did not power off");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="finish-lowbattery", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
