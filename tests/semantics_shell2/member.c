#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/shell/member_pages.c"
#endif
#define key shell2_source_key
#include INK_TEST_SHELL_SOURCE
#undef key

void ink_test_shell2_member(void) {
    ink_shell_member_switch_open(); view("MemberSwitch"); stale("MemberSwitch", "shell.pin_cancel"); close_page();
    ink_shell_member_pin_open("owner"); view("member_gate");
    lv_obj_t *d = find_button(P.card.panel, "1"); event(d, LV_EVENT_PRESSED); event(d, LV_EVENT_CLICKED); view("member_gate");
    static char one[sizeof test_semantic]; snprintf(one, sizeof one, "%s", test_semantic);
    event(d, LV_EVENT_PRESSED); event(d, LV_EVENT_CLICKED); event(d, LV_EVENT_PRESSED); event(d, LV_EVENT_CLICKED); view("member_gate");
    CHECK(!strcmp(one, test_semantic) && strstr(test_semantic, "有值") && !strstr(test_semantic, "shell.pin_submit"), "PIN exports presence only and no AI submit");
    lv_obj_t *forgot = find_button(P.card.panel, "忘记密码"); event(forgot, LV_EVENT_PRESSED);
    stale("member_gate", "shell.pin_forgot"); event(forgot, LV_EVENT_CLICKED);
    CHECK(P.mode == UNLOCK && strlen(P.pin) == 3, "stale form action cannot navigate or change the PIN");
    close_page();
    P.mode = NEW; snprintf(P.name, sizeof P.name, "合成家人"); draw(); view("PinSet"); stale("PinSet", "shell.pin_cancel"); close_page();
    P.mode = FORGOT; draw(); view("PinForgot"); stale("PinForgot", "shell.pin_parent"); close_page();
    P.mode = PARENT; draw(); view("PinForgot"); CHECK(!strstr(test_semantic, "shell.pin_submit"), "parent verification stays local"); close_page();
    P.mode = UNLOCK; P.retry_us = osal_now_us() + 1000000; draw(); view("PinWait"); stale("PinWait", "shell.pin_cancel");
    uint32_t before = ink_ui_generation(); changed(NULL, 0, "member.time_remaining");
    CHECK(ink_ui_generation() == before, "unrelated member event preserves the form"); close_page();
}
