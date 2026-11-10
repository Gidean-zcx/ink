#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/shell/interrupt_card.c"
#endif
#include INK_TEST_SHELL_SOURCE

void ink_test_shell_interrupt(void) {
    CHECK(C.card.root && C.now.id, "real interruption service presented a card");
    semantic_view("interrupt");
    CHECK(strstr(g_snap, "合成打断") && strstr(g_snap, "合成内容") && strstr(g_snap, "test.shell_action"), "visible interruption values and real action are semantic fields");
    uint32_t before = ink_ui_generation(), version = ink_ui_content_version();
    uint32_t revision = saved_revision;
    ink_interrupt_t top = C.now; top.ringing = !top.ringing;
    show(&top);
    CHECK(ink_ui_generation() == before && ink_ui_content_version() == version, "sound-only interruption updates preserve queued input");
    semantic_view("interrupt");
    CHECK(saved_revision == revision, "sound-only update preserves semantic targets");
    lv_obj_t *button = label_button(C.card.row, "合成动作");
    if (button) lv_obj_send_event(button, LV_EVENT_PRESSED, NULL);
    ink_ui_content_changed();
    stale("interrupt", "0");
    click(C.card.row, "合成动作");
    CHECK(ink_shell_test_effects == 0, "stale interruption touch cannot invoke its service action");
    if (button) lv_obj_send_event(button, LV_EVENT_PRESSED, NULL);
    click(C.card.row, "合成动作");
    CHECK(ink_shell_test_effects == 1, "a fresh press still works after unrelated underlying content changes");
    top.actions[0].args[0] = '{'; top.actions[0].args[1] = '}'; top.actions[0].args[2] = 0;
    show(&top);
    semantic_view("interrupt");
    CHECK(saved_revision != revision, "action changes advance the interruption object version");
    show(NULL);
}
