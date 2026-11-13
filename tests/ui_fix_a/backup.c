#include "calls.h"
#define ink_cap_call fx_call
#ifndef INK_UI_FIX_SOURCE
#define INK_UI_FIX_SOURCE "../../system/ui/shell/backup_pages.c"
#endif
#include INK_UI_FIX_SOURCE
#undef ink_cap_call
#include "common.h"
static const char *fx_result;
static void fx_load(void) {
    ink_receipt_t rc = {.status = INK_RC_DONE, .result = fx_result};
    B.pending = false; reply((void *)(uintptr_t)B.generation, &rc);
    if (B.timer) lv_timer_pause(B.timer);
}
static void fx_open(void) { ink_call_t c = {0}; open_page(&c); }
static bool fx_down;
static void fx_pressing(void) { fx_down = ink_ui_pressing(); }
static void fx_resume(void) { if (B.timer) lv_timer_resume(B.timer); }
static bool fx_visible(const char *label) { fx_wanted = label; on_ui(fx_find); return fx_label != NULL; }
/* A status reply that comes while a finger is on the automatic-backup row (small-gaps-3-backup after notify-svc-1:
 * the weekly backup's status and summary redraws fell between press and release, and the tap was lost). */
static void fx_held(void) {
    on_ui(fx_open);
    fx_result = "{\"phase\":\"saved\",\"saved_s\":1794384000}";
    on_ui(fx_load); fx_quiet();
    CHECK(fx_visible("自动备份：关") && fx_visible("已备份到家庭服务器"), "backup page shows the saved state and the schedule row");
    fx_visible("自动备份：关");
    int x = (fx_area.x1 + fx_area.x2) / 2, y = (fx_area.y1 + fx_area.y2) / 2;
    fx_called[0] = fx_args[0] = 0;
    hw(HW_PRESS, x, y);
    CHECK(WAIT_FOR((on_ui(fx_pressing), fx_down), 1000), "the finger is down on the schedule row");
    fx_result = "{\"phase\":\"collecting\"}";
    on_ui(fx_load);
    CHECK(fx_visible("已备份到家庭服务器") && !fx_visible("正在备份"), "a status reply during the press leaves the pressed page in place");
    hw(HW_RELEASE, x, y); delay_ms(200);
    CHECK(!strcmp(fx_called, "settings.set") && strstr(fx_args, "\"backup.auto\""), "the tap still changes the schedule: called %s", fx_called);
    on_ui(fx_resume);
    CHECK(WAIT_FOR(fx_visible("正在备份"), 2500), "the next poll shows the new status once the finger lifts");
}
static void run(void *arg) {
    (void)arg; fx_start(); on_ui(fx_open);
    fx_result = "{\"phase\":\"preview\",\"preview\":\"0123456789abcdef0123456789abcdef\",\"item_count\":27,\"created_s\":1794384000}";
    on_ui(fx_load); fx_capture("preview");
    CHECK(strstr(g_snap, "清单内") && strstr(g_snap, "其他家人不变") && strstr(g_snap, "27"), "UX04 preview identifies bounded replacement and item count");
    const char *phases[] = {"fetching", "preparing", "checking", "restoring"};
    const char *labels[] = {"正在下载备份", "正在整理恢复预览", "正在检查资料", "正在放回资料"};
    for (unsigned i = 0; i < sizeof phases / sizeof *phases; i++) {
        char json[96]; snprintf(json, sizeof json, "{\"phase\":\"%s\"}", phases[i]); fx_result = json;
        on_ui(fx_load); fx_capture(phases[i]);
        CHECK(strstr(g_snap, labels[i]), "UX04 service phase %s is shown literally", phases[i]);
    }
    fx_result = "{\"phase\":\"done\",\"restart_required\":true,\"item_count\":27,\"created_s\":1794384000}";
    on_ui(fx_load); fx_capture("done");
    CHECK(strstr(g_snap, "system.reboot") && strstr(g_snap, "重启"), "UX04 completion offers the real reboot capability");
    fx_press("重启"); CHECK(!strcmp(fx_called, "system.reboot"), "UX04 reboot touch dispatches without executing a reboot");
    fx_result = "{\"phase\":\"failed\",\"next\":2,\"reason\":\"合成测试：写入中断\"}";
    on_ui(fx_load); fx_capture("partial-failure");
    CHECK(strstr(g_snap, "部分资料可能已替换") && strstr(g_snap, "restore.preview"), "UX04 partial failure discloses uncertainty and retries through preview");
    fx_held();
    ink_probe_count("test.finished");
}
void ink_main(void) { product_main(); fx_task(run); }
