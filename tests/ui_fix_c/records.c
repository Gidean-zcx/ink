#ifndef UI_FIX_PRODUCT
#define UI_FIX_PRODUCT "../../system/ui/shell/ai_pages.c"
#endif
#include UI_FIX_PRODUCT
#include "common.h"

static uint32_t fx_records[3];
static void fx_seed(void) {
    ink_db_t *db = ink_db_open_cached_member(ink_db_find_def("activity.log"), "owner");
    CHECK(db, "activity collection available");
    if (!db) return;
    int64_t now = osal_wall_us() / 1000000;
    for (unsigned i = 0; i < 3; i++) {
        char row[384];
        int n = snprintf(row, sizeof row, "{\"cap\":\"%s\",\"status\":\"done\",\"object\":\"%s\",\"revision\":1,\"undo_ms\":%u,\"when\":%lld}",
            i == 2 ? "test.synthetic_ui_record" : "settings.set", i == 0 ? "setting:display.light" : i == 1 ? "setting:audio.volume" : "synthetic:unnamed",
            i == 0 ? 10000 : i == 1 ? 1 : 0, (long long)(i == 1 ? now - 60 : now));
        CHECK(n > 0 && (size_t)n < sizeof row, "synthetic row fits");
        ink_db_write_t rec = {.data = row, .len = (size_t)n, .sort = i == 1 ? now - 60 : now, .src = INK_SRC_APP};
        CHECK(ink_db_put(db, &rec, &fx_records[i]) == 0, "seed activity %u", i);
    }
    ink_db_close(db);
}
static void fx_record_labels(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && (strstr(lv_label_get_text(obj), "setting:") || strstr(lv_label_get_text(obj), "对象名未记录")))
        fx_fits(obj, lv_obj_get_parent(obj));
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) fx_record_labels(lv_obj_get_child(obj, (int32_t)i));
}
static void fx_known(void) {
    if (fx_before) return;
    fx_read(); CHECK(strstr(fx_snapshot, "亮度") && strstr(fx_snapshot, "做好了") && strstr(fx_snapshot, "setting:display.light"), "receipt has actual setting name, result and object");
    CHECK(strstr(fx_snapshot, ":") && !strstr(fx_snapshot, "时间未记录"), "receipt has recorded time");
    CHECK(strstr(fx_snapshot, ink_member_current()->name), "snapshot identifies whose records these are");
    CHECK(fx_label(lv_screen_active(), ink_member_current()->name), "member name is visible at the page header");
    CHECK(strstr(fx_snapshot, "system.undo"), "fresh supported receipt offers undo");
    fx_record_labels(lv_screen_active());
}
static void fx_expired(void) {
    if (fx_before) return;
    fx_read(); CHECK(strstr(fx_snapshot, "已过撤销期限") && !strstr(fx_snapshot, "system.undo"), "expired receipt explains why undo is absent");
    fx_record_labels(lv_screen_active());
}
static void fx_unsupported(void) {
    if (fx_before) return;
    fx_read(); CHECK(strstr(fx_snapshot, "对象名未记录") && strstr(fx_snapshot, "不支持撤销") && strstr(fx_snapshot, "synthetic:unnamed"), "unknown object name and unsupported undo are explicit");
    CHECK(!strstr(fx_snapshot, "test.synthetic_ui_record"), "unknown capability ID is not presented as an operation title");
    fx_record_labels(lv_screen_active());
}
static void fx_log(void) {
    if (fx_before) return;
    fx_read(); CHECK(strstr(fx_snapshot, "对象名未记录") && strstr(fx_snapshot, "做好了"), "log rows also include object and result");
    fx_record_labels(lv_screen_active());
}
static void fx_budget(void) {
    ink_ai_ask_text("合成额度问题");
}
static void fx_budget_check(void) {
    if (fx_before) return;
    fx_read(); CHECK(!strstr(fx_snapshot, "明天") && !strstr(fx_snapshot, "今天的额度") && strstr(fx_snapshot, "额度用完了"), "budget makes no unverified daily-reset promise");
    CHECK(fx_label(lv_layer_sys(), "恢复时间还不确定"), "unknown recovery time is stated on screen");
}
static void run(void *ctx) {
    (void)ctx; fx_start(); fx_ok("activity.list", NULL); fx_seed();
    char args[48]; snprintf(args, sizeof args, "{\"id\":%u}", fx_records[0]);
    fx_ok("shell.ai_receipts", args); fx_capture("records"); on_ui(fx_known);
    snprintf(args, sizeof args, "{\"id\":%u}", fx_records[1]); fx_ok("shell.ai_receipts", args); fx_capture("receipt-expired"); on_ui(fx_expired);
    snprintf(args, sizeof args, "{\"id\":%u}", fx_records[2]); fx_ok("shell.ai_receipts", args); fx_capture("receipt-unsupported"); on_ui(fx_unsupported);
    fx_ok("shell.ai_log", NULL); fx_capture("log"); on_ui(fx_log);
    on_ui(fx_budget); CHECK(WAIT_FOR(!strcmp(ink_ui_top_name(), "AiBudget"), 20000), "synthetic budget rejection arrives"); fx_capture("budget"); on_ui(fx_budget_check);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(fx_options, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "ui-fix-records", .fn = run, .stack_bytes = 24576, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start receipt driver");
}
