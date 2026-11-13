#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/shell/navigation.c"
#endif
#ifndef INK_TEST_SHELL_HOME_SOURCE
#define INK_TEST_SHELL_HOME_SOURCE "../../system/ui/shell/shell_home.c"
#endif
#define ink_ui_semantics_summary inspect_compact_model
#define key shell2_source_key
/* shell_home.c first: navigation.c has a file-scope CAPS that shell_home.c's local CAPS would shadow. */
#include INK_TEST_SHELL_HOME_SOURCE
#include INK_TEST_SHELL_SOURCE
#undef key
#undef ink_ui_semantics_summary

static lv_obj_t *bound(lv_obj_t *root, const char *cap) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, (int)i), *found;
        if (ink_shell_bound_cap(child) && !strcmp(ink_shell_bound_cap(child), cap)) return child;
        if ((found = bound(child, cap))) return found;
    }
    return NULL;
}

void ink_test_shell2_home(void) {
    draw_home_content(); view("home");
    CHECK(strstr(test_semantic, "shell.apps") && strstr(test_semantic, "shell.favorite") && !strstr(test_semantic, "home.next"),
          "the one page names its real capabilities and none to turn screens");
    lv_obj_t *bar = NULL;
    for (uint32_t i = 0; i < lv_obj_get_child_count(N.home); i++) {
        lv_obj_t *child = lv_obj_get_child(N.home, (int)i);
        if (ink_shell_bound_cap(child) && !strcmp(ink_shell_bound_cap(child), "shell.control")) bar = child;
    }
    event(bar, LV_EVENT_PRESSED); stale("home", "shell.control"); event(bar, LV_EVENT_CLICKED);
    CHECK(!N.control, "stale main control tap does not open the panel");
    CHECK(bound(N.content, "shell.apps"), "全部应用 is a row bound to its capability");
    ink_receipt_t opened;
    CHECK(ink_cap_invoke("shell.apps", NULL, INK_SRC_TOUCH, 0, &opened) == INK_RC_DONE, "全部应用 opens its page");
    view("Apps"); CHECK(strstr(test_semantic, "工具页=") && strstr(test_semantic, "shell.icon_menu"), "all apps keeps paging and menu semantics");
    stale("Apps", "shell.apps_next");
    lv_obj_t *icon = bound(lv_screen_active(), "shell.launch");
    event(icon, LV_EVENT_PRESSED); ink_ui_content_changed(); event(icon, LV_EVENT_SHORT_CLICKED);
    CHECK(!strcmp(ink_ui_page_name(), "Apps"), "stale app icon cannot navigate");
    ink_ui_pop(); view("home"); stale("home", "shell.apps");
    uint32_t before = ink_ui_generation(); changed((void *)1, 0, "db.opened");
    CHECK(ink_ui_generation() == before, "unrelated collection event preserves home controls");
    /* A refusal is a passing note, not a state (trial report 2026-11-11: it stayed on the screen); main.c waits
     * for it to go. */
    draw_home_content();
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
void ink_test_shell2_note_check(void) { ink_test_shell2_note_left = N.result[0] || (N.message && strcmp(lv_label_get_text(N.message), idle)); }
