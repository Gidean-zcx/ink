#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/shell/power_menu.c"
#endif
#include INK_TEST_SHELL_SOURCE

void ink_test_shell_power(void) {
    open_job(NULL);
    semantic_view("power_menu");
    CHECK(strstr(g_snap, "system.shutdown") && strstr(g_snap, "system.reboot") && strstr(g_snap, "shell.lock") && strstr(g_snap, "取消"), "power menu keeps its capability names and visible cancel");
    uint32_t before = ink_ui_generation(), version = ink_ui_content_version();
    uint32_t revision = saved_revision;
    on_power(NULL, ink_state_find("power.state"), "power.state");
    CHECK(ink_ui_generation() == before && ink_ui_content_version() == version, "irrelevant power state does not rebuild the menu");
    lv_obj_t *cancel = label_button(M.card.panel, "取消");
    if (cancel) lv_obj_send_event(cancel, LV_EVENT_PRESSED, NULL);
    ink_ui_content_changed();
    stale("power_menu", "system.shutdown");
    click(M.card.panel, "取消");
    CHECK(M.card.root != NULL, "stale power menu touch cannot close the new content");
    close_menu(); open_job(NULL);
    semantic_view("power_menu");
    CHECK(saved_revision != revision, "reopening a power menu invalidates old targets");
    close_menu();
}
