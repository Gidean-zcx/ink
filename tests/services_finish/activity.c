#include "product.h"

#ifndef INK_TEST_CAP_SOURCE
#define INK_TEST_CAP_SOURCE "../../system/policy/cap.c"
#endif
#include INK_TEST_CAP_SOURCE
static int review_ran;
static osal_sem_t *bg_in, *bg_go;
static ink_exec_job_t hold_bg;
static void busy(void *arg) {
    (void)arg; osal_sem_give(bg_in); osal_sem_take(bg_go, OSAL_WAIT_FOREVER);
}
static int pending_count(void) {
    int n = 0;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    for (size_t i = 0; i < sizeof AL.pending / sizeof AL.pending[0]; i++) n += AL.pending[i].used;
    osal_mutex_unlock(CB.lock);
    return n;
}
static void review_action(ink_call_t *c) { (void)c; review_ran++; }
static const ink_cap_t REVIEW_ACTION = {.id="review.action", .title="测试动作",
    .effect=INK_FX_LOCAL_WRITE, .child_ok=true, .exec=INK_EXEC_DIRECT, .run=review_action};
static void review_run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(ink_cap_register(&REVIEW_ACTION, 1) == 0, "register synthetic action");
    int accepted = 0;
    for (int i=0; i<1600; i++) {
        if (!AI("review.action", NULL)) break;
        accepted++; delay_ms(100);
    }
    delay_ms(2200);
    int pending = pending_count();
    ink_db_t *db = ink_db_open(&ACTIVITY, "owner");
    ink_db_cursor_t at = {0}; ink_rec_t rec; size_t bytes = 0;
    while (db && ink_db_next(db, &at, &rec, NULL, 0) == 1) bytes += rec.len;
    ink_probe_value("review.activity_bytes", bytes);
    if (db) ink_db_close(db);
    ink_probe_value("review.actions_before_full", accepted);
    ink_probe_value("review.activity_pending", pending);
    CHECK(accepted == 1600 && bytes <= 128 * 1024 && sim_metric_value("data.dropped_oldest") > 0,
          "capacity is bounded and oldest records are reclaimed: actions=%d bytes=%zu", accepted, bytes);
    CHECK(AI("review.action", NULL), "ordinary AI actions stopped: accepted=%d pending=%d", accepted, pending);
    CHECK(ink_member_switch("kid") == 0, "switch to unaffected member");
    delay_ms(200);
    CHECK(AI("review.action", NULL), "one member's full log blocks the other member too");
    CHECK(BY_HAND("review.action", NULL), "ordinary human action control");
    CHECK(ink_member_switch("owner") == 0, "owner backlog test");
    bg_in = osal_sem_create(0, 1); bg_go = osal_sem_create(0, 1);
    ink_exec_post(INK_EXEC_BG, &hold_bg, busy, NULL);
    CHECK(osal_sem_take(bg_in, 2000000) == OSAL_OK, "background executor held");
    int ran = review_ran;
    for (int i = 0; i < ACTIVITY_PENDING; i++) CHECK(AI("review.action", NULL), "bounded owner reservation %d", i);
    CHECK(!AI("review.action", NULL) && review_ran == ran + ACTIVITY_PENDING, "full member backlog refuses unaudited action");
    CHECK(ink_member_switch("kid") == 0, "other member during storage backlog");
    CHECK(AI("review.action", NULL), "other member retains AI logging capacity");
    CHECK(call_as("review.action", NULL, INK_SRC_CONFIRMED), "other member retains confirmed-action capacity");
    osal_sem_give(bg_go);
    CHECK(WAIT_FOR(pending_count() == 0, 4000), "backlog recovers after storage resumes");
    CHECK(AI("review.action", NULL), "AI actions recover after backlog");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="review-regression", .fn=review_run,
        .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1,
        .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "review task");
}
