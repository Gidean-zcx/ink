/* Locate native keyboard buttons without pretending they are child labels. */
#ifndef INK_TEST_KEYBOARD_MATRIX_H
#define INK_TEST_KEYBOARD_MATRIX_H
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
static lv_obj_t *ink_test_matrix(lv_obj_t *root) {
    if (lv_obj_check_type(root, &lv_keyboard_class)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = ink_test_matrix(lv_obj_get_child(root, (int32_t)i));
        if (found) return found;
    }
    return NULL;
}
static bool ink_test_matrix_key(lv_obj_t *root, const char *text, lv_area_t *area) {
    lv_obj_t *board = ink_test_matrix(root);
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
