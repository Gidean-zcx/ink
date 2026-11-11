#include "calls.h"
#define ink_cap_call fx_call
#ifndef INK_UI_FIX_SOURCE
#define INK_UI_FIX_SOURCE "../../system/ui/shell/factory_pages.c"
#endif
#include INK_UI_FIX_SOURCE
#undef ink_cap_call
#include "common.h"
static void fx_open(void) {
    ink_call_t c = {.cap = ink_cap_find("factory.open")}; navigate(&c);
    if (F.timer) lv_timer_pause(F.timer);
    F.pending = false;
}
static void fx_backup(void) {
    ink_receipt_t rc = {.status = INK_RC_DONE, .result = "{\"saved_s\":1794384000,\"unbacked\":true}"};
    reply((void *)(uintptr_t)F.generation, &rc);
}
static void fx_fit(void) {
    lv_area_t status, buttons;
    lv_obj_update_layout(F.screen);
    lv_obj_get_coords(F.status, &status); lv_obj_get_coords(F.backup_button, &buttons);
    CHECK(status.y2 + ink_ui_px(INK_GAP_L) <= buttons.y1, "UX23 real backup status clears the footer at this font size");
}
static void run(void *arg) {
    (void)arg; fx_start(); on_ui(fx_open); on_ui(fx_backup); fx_capture("review"); on_ui(fx_fit);
    CHECK(!F.media && strstr(g_snap, "clear_media\\\":false") && strstr(g_snap, "存储卡上的文件"), "UX23 all card files are preserved by default");
    CHECK(strstr(g_snap, "之后有东西还没备份") && strstr(g_snap, "shell.back") && strstr(g_snap, "家人"), "UX23 backup state, member and exit are explicit");
    CHECK(ink_cap_find("system.factory_reset")->parent_pin, "UX23 existing reset still requires the person's password");
    fx_press("验证并清空");
    CHECK(!strcmp(fx_called, "system.factory_reset") && strstr(fx_args, "false"), "UX23 explicit action dispatch is captured without clearing data");
    CHECK(sim_metric_value("system.factory_resets") == 0 && ink_storage_size("/user/sys/reset.pending") < 0, "UX23 preview test never starts a reset");
    on_ui(ink_factory_done_open); fx_capture("done");
    CHECK(strstr(g_snap, "重新设置设备") && strstr(g_snap, "factory.finish"), "UX23 completion names the actual next action");
    ink_probe_count("test.finished");
}
void ink_main(void) { product_main(); fx_task(run); }
