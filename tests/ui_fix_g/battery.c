#ifndef UI_FIX_PRODUCT
#define UI_FIX_PRODUCT "../../system/ui/shell/power_pages.c"
#endif
#include UI_FIX_PRODUCT
#include "common.h"
static int mode;
static void render_fixture(void) { B.percent = mode == 3 ? 62 : mode == 1 ? 5 : 2; render(mode); }
static void empty_fixture(void) { B.why = "battery"; draw_off(NULL); }
static void unsaved_fixture(void) { B.why = "battery_unsaved"; draw_off(NULL); }
static void inspect_empty(void) {
    CHECK(strstr(B.body, "充上电再开机") && !strstr(B.body, "充上电就回到这里"), "S29 empty battery consistently asks to charge then power on");
    CHECK(strstr(B.body, "东西都存好了"), "S29 saved result stays truthful");
}
static void inspect_static(lv_obj_t *obj) {
    if (ink_shell_bound_cap(obj) || lv_obj_check_type(obj, &lv_arc_class))
        CHECK(!lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE), "S31/S32 static surfaces expose no clickable control");
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) inspect_static(lv_obj_get_child(obj, (int32_t)i));
}
static void inspect_page(void) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(B.card.panel); i++) inspect_static(lv_obj_get_child(B.card.panel, (int32_t)i));
}
static void inspect_low_icon(void) {
    lv_obj_t *icon = lv_obj_get_child(B.card.panel, 1);
    CHECK(ink_ui_surface_kind(icon) == INK_SURFACE_WHITE && !lv_obj_get_style_border_width(icon, 0), "S30 static battery icon has no button circle");
}
static void run(void *ctx) {
    (void)ctx; fx_start();
    if (!strcmp(fx_variant, "low")) {
        mode = 1; on_ui(render_fixture); fx_capture("battery-low");
        if (!fx_before) on_ui(inspect_low_icon);
        CHECK(strstr(fx_snapshot, "5%") && strstr(fx_snapshot, "interrupt.ack"), "S30 percentage and continue action remain");
    } else if (!strcmp(fx_variant, "saving")) {
        mode = 2; on_ui(render_fixture); fx_capture("battery-saving"); on_ui(inspect_page);
        CHECK(strstr(fx_snapshot, "正在存东西") && !strstr(fx_snapshot, "已关机"), "S31 saving is distinct from powered off");
    } else if (!strcmp(fx_variant, "charging")) {
        mode = 3; on_ui(render_fixture); fx_capture("charging"); on_ui(inspect_page);
        CHECK(strstr(fx_snapshot, "62%") && strstr(fx_snapshot, "AI 键唤醒"), "S32 retained charging percentage and wake hint remain");
    } else if (!strcmp(fx_variant, "unsaved")) {
        on_ui(unsaved_fixture); fx_capture("power-unsaved");
        CHECK(strstr(B.body, "有些内容没保存") && !strstr(B.body, "东西都存好了"), "S29 failed save never claims success");
    } else {
        on_ui(empty_fixture); fx_capture("power-empty"); on_ui(inspect_page);
        if (!fx_before) on_ui(inspect_empty);
    }
    ink_probe_count("test.finished");
}
FX_MAIN("ui-fix-g-battery")
