#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/shell/kid_pages.c"
#endif
#define key shell2_source_key
#include INK_TEST_SHELL_SOURCE
#undef key

void ink_test_shell2_kid(void) {
    show(false); view("KidLimit");
    CHECK(strstr(test_semantic, "shell.kid_parent") && !strstr(test_semantic, "shell.pin_submit"), "parent password is delegated to the local form");
    lv_obj_t *cancel = find_button(K.card.panel, "算了"); event(cancel, LV_EVENT_PRESSED);
    stale("KidLimit", "shell.kid_cancel"); event(cancel, LV_EVENT_CLICKED);
    CHECK(K.card.root, "stale child cancel leaves the card visible");
    close_page(); show(true); view("KidTimeUp");
    CHECK(strstr(test_semantic, "shell.kid_phone") && strstr(test_semantic, "shell.kid_sms"), "time expiry preserves phone and messages");
    stale("KidTimeUp", "shell.kid_add");
    close_page(); show(false);
    uint32_t before = ink_ui_generation(); changed(NULL, 0, "app.cards_rev");
    CHECK(ink_ui_generation() == before, "unrelated cards event does not redraw the child surface"); close_page();
}
