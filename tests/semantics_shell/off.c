#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/shell/off_picture.c"
#endif
#include INK_TEST_SHELL_SOURCE

void ink_test_shell_off(void) {
    O.why = "battery"; draw(NULL);
    semantic_view("off");
    CHECK(strstr(g_snap, O.body) && strstr(g_snap, "已关机") && strstr(g_snap, "\"actions\":[]"), "off picture exports exactly its visible body and no invented actions");
    ink_ui_content_changed();
    stale("off", "close");
    ink_ui_overlay_remove(&OVERLAY); shell_card_close(&O.card);
}
