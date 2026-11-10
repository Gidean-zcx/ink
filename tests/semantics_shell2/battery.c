#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/shell/power_pages.c"
#endif
#define key shell2_source_key
#include INK_TEST_SHELL_SOURCE
#undef key

void ink_test_shell2_battery(void) {
    B.percent = 5; render(1); view("BatteryLow");
    CHECK(strstr(test_semantic, "power.set_saving") && strstr(test_semantic, "interrupt.ack"), "low battery exposes its two real controls");
    ink_interrupt_t it = {.kind = INK_INT_LOW_BATTERY, .nactions = 1, .actions = {{.label = "知道了", .cap = "interrupt.ack"}}};
    int id = ink_interrupt_raise(&it); B.interrupt = id;
    CHECK(id > 0 && ink_interrupt_active(), "real synthetic battery interruption is active");
    lv_obj_t *ack = find_button(B.card.panel, "知道了"); event(ack, LV_EVENT_PRESSED);
    stale("BatteryLow", "interrupt.ack"); event(ack, LV_EVENT_CLICKED);
    CHECK(ink_interrupt_active(), "stale battery acknowledgement cannot end the interruption");
    ink_interrupt_end(id);
    uint32_t before = ink_ui_generation(); changed(NULL, 0, "member.time_remaining");
    CHECK(ink_ui_generation() == before, "unrelated member event preserves battery controls and queued taps");
    B.percent = 2; render(2); view("BatterySaving"); stale("BatterySaving", "interrupt.ack");
    B.percent = 60; render(3); view("Charging"); stale("Charging", "interrupt.ack");
    B.why = "battery"; draw_off(NULL); view("off");
    CHECK(strstr(test_semantic, "PowerOffEmpty") && strstr(test_semantic, "\"actions\":[]"), "off frame has no invented actions");
    stale("off", "interrupt.ack"); close_page(); B.off = false;
}
