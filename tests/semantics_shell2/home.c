#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/shell/navigation.c"
#endif
#define ink_ui_semantics_summary inspect_compact_model
#define key shell2_source_key
#include INK_TEST_SHELL_SOURCE
#undef key
#undef ink_ui_semantics_summary

void ink_test_shell2_home(void) {
    N.home_page = 1; draw_home_content(); view("home");
    CHECK(strstr(test_semantic, "shell.resume") && strstr(test_semantic, "home.next"), "main keeps its real capability names");
    lv_obj_t *dock = NULL;
    for (uint32_t i = 0; i < lv_obj_get_child_count(N.home); i++) {
        lv_obj_t *child = lv_obj_get_child(N.home, (int)i);
        if (ink_shell_bound_cap(child) && !strcmp(ink_shell_bound_cap(child), "shell.control")) dock = child;
    }
    event(dock, LV_EVENT_PRESSED); stale("home", "shell.control"); event(dock, LV_EVENT_CLICKED);
    CHECK(!N.control, "stale main control tap does not open the panel");
    turn(1); view("home"); CHECK(strstr(test_semantic, "工具页=") && strstr(test_semantic, "shell.icon_menu"), "apps keeps paging and menu semantics"); stale("home", "home.prev");
    lv_obj_t *icon = NULL;
    for (uint32_t i = 0; i < lv_obj_get_child_count(N.content); i++) {
        lv_obj_t *child = lv_obj_get_child(N.content, (int)i);
        const char *cap = ink_shell_bound_cap(child);
        if (cap && !strcmp(cap, "shell.launch")) { icon = child; break; }
    }
    event(icon, LV_EVENT_PRESSED); ink_ui_content_changed(); event(icon, LV_EVENT_SHORT_CLICKED);
    CHECK(N.home && !strcmp(ink_ui_page_name(), "home"), "stale app icon cannot navigate");
    turn(-1); turn(-1); view("home"); stale("home", "home.next");
    uint32_t before = ink_ui_generation(); changed((void *)1, 0, "db.opened");
    CHECK(ink_ui_generation() == before, "unrelated collection event preserves home controls");
    /* A refusal is a passing note, not a state (trial report 2026-11-11: it stayed on the screen); main.c waits
     * for it to go. */
    N.home_page = 1; draw_home_content();
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    CHECK(args != NULL, "launch arguments");
    if (!args) return;
    ink_args_init(args); ink_args_str(args, "id", "nosuchapp");
    ink_receipt_t rc;
    ink_cap_invoke("shell.launch", args, INK_SRC_TOUCH, 0, &rc);
    osal_free(args);
    CHECK(rc.status == INK_RC_REJECTED && N.message && !strcmp(lv_label_get_text(N.message), TR("这个应用还没有可打开的内容")) &&
          N.note_timer, "an app with nothing to open leaves a passing note");
}
bool ink_test_shell2_note_left;
void ink_test_shell2_note_check(void) { ink_test_shell2_note_left = N.result[0] || (N.message && lv_label_get_text(N.message)[0]); }
