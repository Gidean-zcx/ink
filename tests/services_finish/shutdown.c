#include "product.h"
#ifndef INK_TEST_POWER_SOURCE
#define INK_TEST_POWER_SOURCE "../../system/services/power/power_service.c"
#endif
#include INK_TEST_POWER_SOURCE
static const ink_db_def_t KEPT = {.name="finish.shutdown", .scope=INK_DB_DEVICE, .schema=1};
static ink_db_t *db;
extern void (*sim_power_before_shutdown)(void);
static void before_shutdown(void) {
    CHECK(!ink_audio_recording() && !ink_audio_capture_busy(), "AUD18-203: recording finished before poweroff");
    CHECK(ink_db_durable_ver(db) == ink_db_last_ver(db) && ink_db_idle() && !ink_storage_pending(), "AUD18-203: accepted edit durable before poweroff");
    ink_probe_count("test.finished");
}
static void run(void *arg) {
    (void)arg;
    delay_ms(2200);
    CHECK(ink_db_register(&KEPT) == 0 && (db = ink_db_open(&KEPT, NULL)), "collection");
    CHECK(BY_HAND("audio.record", "{\"path\":\"/me/shutdown.opus\"}") && g_rc.status == INK_RC_DONE, "record");
    delay_ms(1000);
    ink_db_write_t w = {.data="SYNTHETIC", .len=9, .flags=INK_DB_LAZY};
    CHECK(ink_db_put(db, &w, NULL) == INK_DB_OK, "edit accepted");
    sim_power_before_shutdown = before_shutdown;
    BY_HAND("system.shutdown", NULL);
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="finish-shutdown", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
