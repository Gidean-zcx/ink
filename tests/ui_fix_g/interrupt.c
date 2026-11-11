#ifndef UI_FIX_PRODUCT
#define UI_FIX_PRODUCT "../../system/ui/shell/interrupt_card.c"
#endif
#include UI_FIX_PRODUCT
#include "common.h"
static ink_interrupt_t interruption;
static void show_fixture(void) { show(&interruption); }
static void clear_fixture(void) { show(NULL); }
static void inspect_context(void) {
    lv_obj_t *label = fx_label(C.card.panel, "结束后回到刚才的页面");
    CHECK(label, "Z07 interrupt states the return destination");
    if (label) fx_fits(label, C.card.panel);
    CHECK(!lv_obj_has_flag(C.icon, LV_OBJ_FLAG_CLICKABLE) && !lv_obj_get_style_border_width(C.icon, 0), "Z07 static interrupt icon has no button outline or action");
}
static void run(void *ctx) {
    (void)ctx; fx_start(); char original[64]; snprintf(original, sizeof original, "%s", ink_ui_page_name());
    interruption = (ink_interrupt_t){.id = 4242, .kind = INK_INT_CALL, .nactions = 2,
        .title = "测试来电", .body = "10000000000", .actions = {{.label = "接听", .cap = "call.answer"}, {.label = "拒接", .cap = "call.hangup"}}};
    on_ui(show_fixture); fx_capture("interrupt-call");
    if (!fx_before) on_ui(inspect_context);
    on_ui(show_fixture); CHECK(C.now.id == 4242, "Z07 same interruption is preserved"); on_ui(clear_fixture);
    interruption = (ink_interrupt_t){.id = 4243, .kind = INK_INT_ALARM, .nactions = 2,
        .title = "闹钟", .body = "合成提醒", .actions = {{.label = "关闭", .cap = "alarm.dismiss"}, {.label = "再睡 10 分钟", .cap = "alarm.snooze"}}};
    on_ui(show_fixture); fx_capture("interrupt-alarm");
    if (!fx_before) on_ui(inspect_context);
    on_ui(clear_fixture); fx_quiet();
    CHECK(!C.card.root && !strcmp(original, ink_ui_page_name()), "Z07 dismissal leaves original activity unchanged");
    ink_probe_count("test.finished");
}
FX_MAIN("ui-fix-g-interrupt")
