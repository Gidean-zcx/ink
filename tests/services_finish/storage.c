#include "product.h"
#include "ink/sys/svc.h"
#ifndef INK_TEST_DATA_SOURCE
#define INK_TEST_DATA_SOURCE "../../system/services/data/datastore.c"
#endif
#include INK_TEST_DATA_SOURCE
static int sentinel;
static ink_svc_deadline_t deadline;
static void tick(void *ctx) { (void)ctx; sentinel++; }
static const ink_db_def_t DEF = {.name="finish.lazy", .scope=INK_DB_MEMBER, .schema=1, .json=true, .ai_read=true, .ai_write=true};
static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    CHECK(ink_db_register(&DEF) == 0, "register");
    ink_db_t *owner = ink_db_open(&DEF, "owner"), *kid = ink_db_open(&DEF, "kid");
    CHECK(owner && kid, "collections");
    ink_db_write_t w = {.data="{\"v\":1}", .len=7, .flags=INK_DB_LAZY};
    CHECK(ink_db_put(owner, &w, NULL) == 0, "owner lazy record");
    ink_svc_at(&deadline, osal_now_us() + 1000000, tick, NULL);
    CHECK(ink_db_put(kid, &w, NULL) == 0, "kid lazy record");
    ink_db_close(kid);
    ink_db_forget_member("kid");
    delay_ms(1500);
    CHECK(sentinel == 1, "AUD18-109: unrelated timer survives (%d)", sentinel);
    ink_power_hold(INK_NEED_NO_LOCK);
    CHECK(WAIT_FOR(ink_db_durable_ver(owner) == ink_db_last_ver(owner), 63000), "AUD18-109: unrelated lazy data becomes durable");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="finish-storage", .fn=run, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
