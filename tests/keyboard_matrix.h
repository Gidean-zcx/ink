/* Locate native keyboard buttons without pretending they are child labels. */
#ifndef INK_TEST_KEYBOARD_MATRIX_H
#define INK_TEST_KEYBOARD_MATRIX_H
#include "ink/sys/cap.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
static lv_obj_t *ink_test_matrix(lv_obj_t *root) {
    if (lv_obj_check_type(root, &lv_keyboard_class)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = ink_test_matrix(lv_obj_get_child(root, (int32_t)i));
        if (found) return found;
    }
    return NULL;
}
static bool ink_test_keyboard_label(lv_obj_t *root, const char *label, lv_area_t *area) {
    if (!lv_obj_is_visible(root)) return false;
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), label)) {
        lv_obj_get_coords(lv_obj_get_parent(root), area);
        return true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++)
        if (ink_test_keyboard_label(lv_obj_get_child(root, (int32_t)i), label, area)) return true;
    return false;
}
static bool ink_test_matrix_key(lv_obj_t *root, const char *text, lv_area_t *area) {
    lv_obj_t *board = ink_test_matrix(root);
    if (board && !strcmp(text, "更多")) return ink_test_keyboard_label(root, "▾", area);
    if (board && !strcmp(text, "字母")) {
        if (ink_test_keyboard_label(root, "26", area)) return true;
        text = "26";
    }
    lv_area_t navigation;
    if (board && !lv_obj_is_visible(board) && text[0] && !text[1] &&
        ink_test_keyboard_label(root, "123", &navigation)) {
        /* ASCII symbols now live on the English tab's explicit pages. */
        ink_receipt_t rc;
        if (text[0] >= '0' && text[0] <= '9') {
            if (ink_cap_invoke_json("keyboard.panel", "{\"panel\":\"number\"}", INK_SRC_TOUCH, 0, &rc) != INK_RC_DONE) return false;
            lv_obj_update_layout(root);
            return ink_test_matrix_key(root, text, area);
        }
        if (ink_cap_invoke_json("keyboard.category", "{\"category\":2}", INK_SRC_TOUCH, 0, &rc) != INK_RC_DONE) return false;
        for (int page = 0; page < 4; page++) {
            lv_obj_update_layout(root);
            if (ink_test_keyboard_label(root, text, area)) return true;
            if (ink_cap_invoke_json("keyboard.symbol_page", "{\"direction\":1}", INK_SRC_TOUCH, 0, &rc) != INK_RC_DONE) break;
        }
        return false;
    }
    if (!board || !lv_obj_is_visible(board)) return false;
    lv_buttonmatrix_t *matrix = (lv_buttonmatrix_t *)board;
    for (uint32_t i = 0; i < matrix->btn_cnt; i++) {
        if (strcmp(lv_keyboard_get_button_text(board, i), text) ||
            lv_buttonmatrix_has_button_ctrl(board, i, LV_BUTTONMATRIX_CTRL_HIDDEN)) continue;
        lv_area_t bounds; lv_obj_get_coords(board, &bounds);
        *area = matrix->button_areas[i];
        area->x1 += bounds.x1; area->x2 += bounds.x1;
        area->y1 += bounds.y1; area->y2 += bounds.y1;
        return true;
    }
    if (board == root) return false;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++)
        if (ink_test_matrix_key(lv_obj_get_child(root, (int32_t)i), text, area)) return true;
    return false;
}
#endif
