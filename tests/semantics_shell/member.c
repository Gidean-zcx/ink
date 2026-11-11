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
    const ink_cap_t *select = ink_cap_find("shell.member_select");
    CHECK(select && !ink_cap_ai_callable(select), "member selection is registered and human-only");
    ink_receipt_t rc;
    CHECK(ink_cap_invoke_json("shell.member_select", "{\"id\":\"kid\"}", INK_SRC_AI,
        ink_ui_content_version(), &rc) != INK_RC_DONE && !strcmp(ink_member_current()->id, "owner"), "AI cannot select an identity");
    double before_select = sim_metric_value("action.user.shell.member_select");
    if (member) lv_obj_send_event(member, LV_EVENT_PRESSED, NULL);
    click(G.card.panel, "小");
    CHECK(sim_metric_value("action.user.shell.member_select") == before_select + 1 &&
        !strcmp(ink_member_current()->id, "kid"), "avatar tap goes through the atomic selection capability");
    CHECK(ink_member_switch("owner") == 0, "return to the synthetic adult");
    G.requested = false; sync_gate(NULL);
}
