#ifndef UI_FIX_PRODUCT
#define UI_FIX_PRODUCT "../../system/ui/shell/power_menu.c"
#endif
#include UI_FIX_PRODUCT
#include "common.h"
static void open_fixture(void) { open_job(NULL); }
static void inspect_menu(void) {
    lv_obj_t *report = fx_label(M.card.panel, "记录问题"), *export = fx_label(M.card.panel, "导出日志");
    CHECK(report && export, "S27 diagnostics remain available");
    if (!report || !export) return;
    lv_area_t a, b; lv_obj_get_coords(lv_obj_get_parent(report), &a); lv_obj_get_coords(lv_obj_get_parent(export), &b);
    CHECK(b.y1 - a.y2 - 1 >= ink_ui_px(INK_GAP), "S27 diagnostic buttons have the required 19 pixel gap");
    CHECK(fx_label(M.card.panel, "帮助排查"), "S27 diagnostic group is separate from daily power actions");
    fx_fits(report, lv_obj_get_parent(report)); fx_fits(export, lv_obj_get_parent(export));
    lv_obj_t *cancel = fx_label(M.card.panel, "取消");
    fx_fits(cancel, lv_obj_get_parent(cancel));
    lv_obj_get_coords(lv_obj_get_parent(cancel), &a);
    CHECK(a.y2 <= ink_ui_px(INK_CANVAS_H - INK_MARGIN), "S27 cancel preserves bottom margin");
    CHECK(a.y1 > 0, "S27 complete menu remains on screen");
}
static void inspect_result(void) {
    CHECK(M.say[0], "S27 diagnostic action reports its actual receipt");
    fx_fits(M.result, M.card.panel);
    lv_area_t a, b; lv_obj_get_coords(M.result, &a); lv_obj_get_coords(lv_obj_get_parent(fx_label(M.card.panel, "取消")), &b);
    CHECK(a.y2 < b.y1, "S27 result never overlaps cancel");
}
static void run(void *ctx) {
    (void)ctx; fx_start(); on_ui(open_fixture); fx_capture("power-menu");
    if (!fx_before) on_ui(inspect_menu);
    fx_press("记录问题"); CHECK(WAIT_FOR(M.say[0], 5000), "problem recording reply arrives"); fx_capture("power-recorded");
    if (!fx_before) on_ui(inspect_result);
    fx_press("导出日志"); fx_quiet(); fx_capture("power-exported");
    if (!fx_before) on_ui(inspect_result);
    fx_press("取消"); CHECK(!M.card.root, "S27 cancel returns to original page");
    ink_probe_count("test.finished");
}
FX_MAIN("ui-fix-g-menu")
