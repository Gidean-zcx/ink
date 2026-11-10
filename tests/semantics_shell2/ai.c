#include "common.h"
#ifndef INK_TEST_SHELL_SOURCE
#define INK_TEST_SHELL_SOURCE "../../system/ui/shell/ai_panel.c"
#endif
#define key shell2_source_key
#include INK_TEST_SHELL_SOURCE
#undef key

void ink_test_shell2_ai(void) {
    lv_obj_t *root, *status, *question, *answer;
    ink_shell_ai_open(&root, &status, &question, &answer);
    ink_shell_ai_words("合成回答"); view("Ai");
    CHECK(strstr(test_semantic, "answer") && strstr(test_semantic, "合成回答") && strstr(test_semantic, "ai.panel.hide"), "AI panel preserves its field and action ids");
    lv_obj_t *hide = NULL;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, (int)i);
        const char *cap = ink_shell_bound_cap(child);
        if (cap && !strcmp(cap, "ai.panel.hide")) hide = child;
    }
    event(hide, LV_EVENT_PRESSED);
    ink_shell_ai_words("合成回答补充");
    event(hide, LV_EVENT_CLICKED);
    CHECK(!A.hidden, "a changed answer rejects the old hide tap");
    view("Ai"); stale("home", "ai.panel.hide");
    event(hide, LV_EVENT_PRESSED); event(hide, LV_EVENT_CLICKED);
    /* Calls on the same executor may complete inline; either receipt path is valid. */
    ink_shell_ai_closed(); lv_obj_delete(root);
}
