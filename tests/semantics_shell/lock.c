#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/lock_mark.c"
#endif
#include INK_TEST_SHELL_SOURCE

void ink_test_shell_lock(void) {
    show();
    semantic_view("LockPage");
    CHECK(g_label && strstr(g_snap, lv_label_get_text(g_label)) && strstr(g_snap, "\"id\":\"owner\",\"role\":\"button\"") && !strstr(g_snap, "shell.lock_enter"),
          "lock snapshot shares the visible status and the family, and offers the AI no way in");
    uint32_t before = ink_ui_content_version();
    on_audio(NULL, 0, "media.state");
    CHECK(ink_ui_content_version() == before, "unchanged listening status does not invalidate input");
    ink_ui_content_changed();
    stale("LockPage", "owner");
    lv_obj_t *picture = NULL;
    for (uint32_t i = 0; i < lv_obj_get_child_count(g_mark); i++) {
        lv_obj_t *child = lv_obj_get_child(g_mark, (int)i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_CLICKABLE) && lv_obj_get_width(child) == ink_ui_px(INK_CIRCLE)) picture = child;
    }
    CHECK(picture != NULL, "lock picture control exists");
    ink_power_state_t state = ink_power_state();
    if (picture) lv_obj_send_event(picture, LV_EVENT_CLICKED, NULL);
    CHECK(!C.pending[0] && ink_power_state() == state, "a stale picture touch cannot unlock or enter");
    uint32_t revision = saved_revision;
    hide(); show(); semantic_view("LockPage");
    CHECK(saved_revision != revision, "a new lock surface invalidates the previous picture target");
    hide();
}
