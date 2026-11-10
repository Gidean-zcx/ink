#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/shell/member_gate.c"
#endif
#include INK_TEST_SHELL_SOURCE

void ink_test_shell_member(void) {
    G.requested = true; sync_gate(NULL);
    semantic_view("member_gate");
    CHECK(strstr(g_snap, "选择家人") && strstr(g_snap, "小朋友") && !strstr(g_snap, "\"capability\":\"member.switch\""), "member controls export their visible labels without AI identity switching");
    uint32_t before = ink_ui_generation();
    sync_gate(NULL);
    CHECK(ink_ui_generation() == before, "unchanged member state does not rebuild the gate");
    lv_obj_t *member = label_button(G.card.panel, "小");
    if (member) lv_obj_send_event(member, LV_EVENT_PRESSED, NULL);
    ink_ui_content_changed();
    stale("member_gate", "kid");
    click(G.card.panel, "小");
    CHECK(!strcmp(ink_member_current()->id, "owner"), "stale member touch cannot change identity");
    G.requested = false; sync_gate(NULL);
}
