#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/shell/confirm_card.c"
#endif
#include INK_TEST_SHELL_SOURCE

void ink_test_shell_confirm(void) {
    CHECK(C.card.root && C.now.token, "real confirmation service presented a card");
    semantic_view("confirm");
    CHECK(strstr(g_snap, "家人=owner") && strstr(g_snap, "确认编号=") && strstr(g_snap, "阅读=") && strstr(g_snap, "排队=") &&
          strstr(g_snap, "confirm.withdraw") && !strstr(g_snap, "confirm.accept") && !strstr(g_snap, "confirm.reject"), "confirmation context and owner-only boundaries survive migration");
    uint32_t version = saved_version, revision = saved_revision;
    C.now.parent = true;
    parent_page();
    semantic_view("confirm");
    CHECK(saved_version == version && saved_revision != revision, "PIN navigation advances the object without invalidating the underlying confirmed action");
    CHECK(strstr(g_snap, "\"id\":\"pin\"") && strstr(g_snap, "\"value\":\"无值\",\"secret\":true"), "empty PIN exposes presence only");
    click(C.pad, "1");
    semantic_view("confirm");
    CHECK(strstr(g_snap, "\"value\":\"有值\",\"secret\":true"), "typed PIN exposes presence only");
    static char one_digit[sizeof g_snap];
    snprintf(one_digit, sizeof one_digit, "%s", g_snap);
    for (int i = 0; i < 3; i++) click(C.pad, "1");
    semantic_view("confirm");
    CHECK(!strcmp(one_digit, g_snap), "PIN length and contents do not affect the exported model");
    lv_obj_t *cancel = label_button(C.pad, "取消");
    if (cancel) lv_obj_send_event(cancel, LV_EVENT_PRESSED, NULL);
    ink_ui_content_changed();
    stale("confirm", "reject");
    click(C.pad, "取消");
    ink_confirm_view_t pending;
    CHECK(ink_confirm_list(&pending, 1) == 1 && pending.token == C.now.token, "stale confirmation touch cannot reject the transaction");
    close_card();
}
