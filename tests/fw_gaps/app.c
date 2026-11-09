#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

int ink_fw_gap_refs(const char *member);
static const ink_db_def_t DATA = {.name = "gap.entries", .title = "合成集合", .scope = INK_DB_MEMBER, .schema = 1};
static const ink_db_def_t BAD_DATA = {.name = "gap.invalid/name", .scope = INK_DB_MEMBER};
static const ink_state_def_t STATE = {.id = "test.gap", .title = "合成状态", .type = INK_ST_INT};
static const ink_setting_def_t SETTING = {.id = "test.gap", .title = "合成设置", .type = INK_SET_INT, .min = 0, .max = 1000};
static int state_h, handles[3], old_handles[3], values, counts[3], replacements[3];
static int opens, errors;
static lv_obj_t *page;
static ink_exec_t executor;
static ink_exec_job_t job, hold_job;
static osal_sem_t *gate, *entered, *finished;
static char owner[16];
static void opened(void *ctx, ink_db_t *db) {
    (void)ctx;
    CHECK(ink_exec_is_current(INK_EXEC_UI), "open completion belongs to UI");
    if (db) opens++;
    else errors++;
    ink_db_close(db);
}
static void begin_open(void) {
    page = lv_obj_create(lv_layer_sys());
    CHECK(ink_app_db_open_async("gap", &DATA, page, opened, NULL) == 0, "accept background open");
}
static void close_early(void) { begin_open(); lv_obj_delete(page); page = NULL; }
static void switch_early(void) { begin_open(); CHECK(ink_member_switch("kid") == 0, "change member before completion"); }
static void delete_page(void) { lv_obj_delete(page); page = NULL; }
static void ui_barrier(void) {}
static void fail_open(void) {
    page = lv_obj_create(lv_layer_sys());
    CHECK(ink_app_db_open_async("gap", &BAD_DATA, page, opened, NULL) == 0, "accept open that will fail");
    CHECK(ink_app_db_open_async("other", &DATA, page, opened, NULL) == -1, "reject another app's collection");
}
static void hold_bg(void *arg) {
    (void)arg;
    osal_sem_give(entered);
    CHECK(osal_sem_take(gate, OSAL_WAIT_FOREVER) == OSAL_OK, "release the existing BG executor");
}
static void state_changed(void *ctx, int h, const char *id) {
    (void)h; (void)id;
    CHECK(ink_exec_is_current(executor), "state/settings callback uses selected executor");
    ((int *)ctx)[0]++;
}
static void event_changed(void *ctx, const char *id, const ink_args_t *payload) {
    (void)id; (void)payload;
    CHECK(ink_exec_is_current(executor), "event callback uses selected executor");
    ((int *)ctx)[0]++;
}
static void changes(void) {
    ink_receipt_t rc;
    ink_state_set_int(state_h, ++values);
    CHECK(ink_setting_set("test.gap", values, INK_SRC_SYSTEM, &rc) == INK_RC_DONE, "change synthetic setting");
    ink_cap_emit("test.gap", NULL);
}
static void subscribe_to(int *target) {
    handles[0] = ink_state_watch_handle("test.gap", state_changed, &target[0], executor);
    handles[1] = ink_settings_watch_handle("test.gap", state_changed, &target[1], executor);
    handles[2] = ink_cap_subscribe_handle("test.gap", event_changed, &target[2], executor);
    CHECK(handles[0] > 0 && handles[1] > 0 && handles[2] > 0, "three removable handles");
}
static void unsubscribe_all(void) {
    CHECK(ink_state_unwatch(handles[0]) == 0 && ink_settings_unwatch(handles[1]) == 0 &&
          ink_cap_unsubscribe(handles[2]) == 0, "unsubscribe on the selected executor after callbacks finish");
}
static void start(void *arg) { (void)arg; subscribe_to(counts); changes(); osal_sem_give(finished); }
static void replace(void *arg) {
    (void)arg;
    CHECK(counts[0] == 1 && counts[1] == 1 && counts[2] == 1, "all active callbacks ended before unsubscription");
    changes(); /* Delivery is queued behind this job. */
    memcpy(old_handles, handles, sizeof handles);
    unsubscribe_all(); subscribe_to(replacements);
    CHECK(ink_state_unwatch(old_handles[0]) == -1 && ink_settings_unwatch(old_handles[1]) == -1 &&
          ink_cap_unsubscribe(old_handles[2]) == -1, "stale handles cannot cancel replacements");
    osal_sem_give(finished);
}
static void emit_again(void *arg) { (void)arg; changes(); osal_sem_give(finished); }
static void stop(void *arg) { (void)arg; unsubscribe_all(); changes(); osal_sem_give(finished); }
static void barrier(void *arg) { (void)arg; osal_sem_give(finished); }
static void at_executor(void (*fn)(void *)) {
    CHECK(ink_exec_post(executor, &job, fn, NULL) == 0 && osal_sem_take(finished, OSAL_WAIT_FOREVER) == OSAL_OK,
          "executor job finished");
}
static int capacity(int kind) {
    int h[64], n = 0;
    while (n < 64) {
        int v = kind == 0 ? ink_state_watch_handle("unused.", state_changed, counts, INK_EXEC_UI) :
                kind == 1 ? ink_settings_watch_handle("unused.", state_changed, counts, INK_EXEC_UI) :
                            ink_cap_subscribe_handle("unused.", event_changed, counts, INK_EXEC_UI);
        if (v < 0) break;
        h[n++] = v;
    }
    for (int i = 0; i < n; i++) {
        int r = kind == 0 ? ink_state_unwatch(h[i]) : kind == 1 ? ink_settings_unwatch(h[i]) : ink_cap_unsubscribe(h[i]);
        CHECK(r == 0, "release each capacity probe");
    }
    return n;
}
static void create(lv_obj_t *screen, void *arg) { (void)screen; (void)arg; subscribe_to(counts); }
static const ink_activity_t PAGE = {.name = "fw.gaps", .create = create, .destroy = unsubscribe_all};
static void repeat_pages(void) {
    int before[3] = {capacity(0), capacity(1), capacity(2)};
    for (int i = 0; i < 100; i++) { ink_ui_push(&PAGE, NULL); changes(); ink_ui_pop(); }
    CHECK(capacity(0) == before[0] && capacity(1) == before[1] && capacity(2) == before[2],
          "100 real page creations/destructions do not consume subscription slots");
    CHECK(ink_state_watch("legacy.", state_changed, counts, INK_EXEC_DIRECT) == 0 &&
          ink_settings_watch("legacy.", state_changed, counts, INK_EXEC_DIRECT) == 0 &&
          ink_cap_subscribe("legacy.", event_changed, counts, INK_EXEC_DIRECT) == 0, "legacy success remains zero");
    CHECK(ink_state_watch_handle("", state_changed, counts, INK_EXEC_DIRECT) == -1 &&
          ink_settings_watch_handle("", state_changed, counts, INK_EXEC_DIRECT) == -1 &&
          ink_cap_subscribe_handle("", event_changed, counts, INK_EXEC_DIRECT) == -1, "removable watches require a serial executor");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home();
    snprintf(owner, sizeof owner, "%s", ink_member_current()->id);
    on_ui(begin_open);
    CHECK(WAIT_FOR(opens == 1, 10000) && ink_fw_gap_refs(owner) == 0, "normal completion transfers exactly one reference");
    on_ui(delete_page); on_ui(close_early);
    at_executor(barrier); /* BG finishes opening before the UI drain below. */
    on_ui(ui_barrier);
    CHECK(WAIT_FOR(ink_fw_gap_refs(owner) == 0, 10000) && opens == 1, "closed page gets no callback and leaks no reference");
    CHECK(ink_exec_post(INK_EXEC_BG, &hold_job, hold_bg, NULL) == 0 && osal_sem_take(entered, OSAL_WAIT_FOREVER) == OSAL_OK,
          "hold only the existing executor to fix the identity handoff order");
    on_ui(switch_early); osal_sem_give(gate);
    at_executor(barrier);
    on_ui(ui_barrier);
    CHECK(WAIT_FOR(ink_fw_gap_refs(owner) == 0, 10000) && ink_fw_gap_refs("kid") == -1 && opens == 1,
          "open uses the captured adult identity, then closes without handing it to the child");
    on_ui(delete_page); ink_member_test_reset();
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "restore synthetic adult");
    on_ui(fail_open);
    CHECK(WAIT_FOR(errors == 1, 10000) && opens == 1, "open failure reaches UI with a NULL handle");
    on_ui(delete_page);
    state_h = ink_state_define(&STATE);
    CHECK(state_h >= 0 && ink_settings_define(&SETTING, 1) == 0, "define synthetic state and setting");
    for (int e = INK_EXEC_UI; e < INK_EXEC_COUNT; e++) {
        if (e == INK_EXEC_DIRECT) continue;
        executor = (ink_exec_t)e;
        memset(counts, 0, sizeof counts); memset(replacements, 0, sizeof replacements);
        at_executor(start);
        CHECK(WAIT_FOR(counts[0] == 1 && counts[1] == 1 && counts[2] == 1, 10000), "all three subscriptions deliver");
        CHECK(ink_state_unwatch(handles[0]) == -1 && ink_settings_unwatch(handles[1]) == -1 && ink_cap_unsubscribe(handles[2]) == -1,
              "wrong executor cannot destroy callback context");
        at_executor(replace); at_executor(barrier);
        CHECK(counts[0] == 1 && counts[1] == 1 && counts[2] == 1 && !replacements[0] && !replacements[1] && !replacements[2],
              "queued old callbacks reach neither retired nor replacement subscriptions");
        at_executor(emit_again);
        CHECK(WAIT_FOR(replacements[0] == 1 && replacements[1] == 1 && replacements[2] == 1, 10000), "replacement receives only new changes");
        at_executor(stop); at_executor(barrier);
        CHECK(replacements[0] == 1 && replacements[1] == 1 && replacements[2] == 1, "no delivery after unsubscribe");
    }
    executor = INK_EXEC_UI;
    memset(counts, 0, sizeof counts);
    on_ui(repeat_pages); at_executor(barrier);
    CHECK(!counts[0] && !counts[1] && !counts[2], "destroyed pages receive no queued callbacks");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    gate = osal_sem_create(0, 1); entered = osal_sem_create(0, 1); finished = osal_sem_create(0, 1);
    executor = INK_EXEC_BG;
    osal_task_cfg_t cfg = {.name = "fw-gaps-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(gate && entered && finished && osal_task_create(&cfg, NULL) == OSAL_OK, "start test driver");
}
