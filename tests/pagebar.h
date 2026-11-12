/* Match the shared pager's exact page state, including its one-to-five dot form. */
#include "ink/ui_theme.h"
static lv_obj_t *ink_test_pagebar(lv_obj_t *root, int page, int total) {
    if (!lv_obj_is_visible(root)) return NULL;
    if (lv_obj_get_child_count(root) == 3) {
        lv_obj_t *indicator = lv_obj_get_child(root, 1);
        if (lv_obj_check_type(indicator, &lv_label_class) && lv_obj_get_width(indicator) == ink_ui_px(INK_CIRCLE_LARGE) &&
            lv_obj_get_height(root) == ink_ui_px(INK_CONTROL)) {
            char text[24] = "";
            if (total > 5) snprintf(text, sizeof text, "%d/%d", page, total);
            bool matches = !strcmp(lv_label_get_text(indicator), text) &&
                lv_obj_get_child_count(indicator) == (uint32_t)(total <= 5 ? total : 0);
            for (int i = 0; matches && total <= 5 && i < total; i++)
                matches = (ink_ui_surface_kind(lv_obj_get_child(indicator, i)) == INK_SURFACE_BLACK) == (i + 1 == page);
            if (matches) return root;
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *bar = ink_test_pagebar(lv_obj_get_child(root, (int32_t)i), page, total);
        if (bar) return bar;
    }
    return NULL;
}
