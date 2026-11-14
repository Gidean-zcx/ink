/* Locate native keyboard buttons without pretending they are child labels; only what is visible is found. */
#ifndef INK_TEST_KEYBOARD_MATRIX_H
#define INK_TEST_KEYBOARD_MATRIX_H
#include <stdio.h>
#include "ink/sys/i18n.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
static lv_obj_t *ink_test_matrix(lv_obj_t *root) {
    if (lv_obj_check_type(root, &lv_keyboard_class)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = ink_test_matrix(lv_obj_get_child(root, (int32_t)i));
        if (found) return found;
    }
    return NULL;
}
/* A visible keyboard button carrying this label (symbol cells, tabs, page buttons, readings, candidates). */
static lv_obj_t *ink_test_keyboard_label(lv_obj_t *root, const char *text, bool underlined) {
    if (!lv_obj_is_visible(root)) return NULL;
    if (lv_obj_check_type(root, &lv_label_class) && (text ? !strcmp(lv_label_get_text(root), text) :
        underlined && lv_obj_get_style_text_decor(root, 0) == LV_TEXT_DECOR_UNDERLINE)) return lv_obj_get_parent(root);
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) { /* the last one: the page row comes after the symbols */
        lv_obj_t *found = ink_test_keyboard_label(lv_obj_get_child(root, (int32_t)(lv_obj_get_child_count(root) - 1 - i)), text, underlined);
        if (found) return found;
    }
    return NULL;
}
static bool ink_test_matrix_key(lv_obj_t *root, const char *text, lv_area_t *area) {
    lv_obj_t *board = ink_test_matrix(root);
    /* two keys were renamed by keyboard-2: 更多 is the ▾ beside the candidates, 字母 is 26 on the panels */
    if (!strcmp(text, "更多") && !ink_test_keyboard_label(lv_layer_sys(), "更多", false)) text = "▾";
    else if (!strcmp(text, "字母") && !ink_test_keyboard_label(lv_layer_sys(), "字母", false)) text = "26";
    if (board && root == lv_layer_sys()) { /* the keyboard's own buttons */
        lv_obj_t *button = ink_test_keyboard_label(root, text, false);
        if (button && !lv_obj_check_type(lv_obj_get_parent(button), &lv_keyboard_class) && lv_obj_check_type(button, &lv_button_class)) {
            lv_obj_get_coords(button, area);
            return true;
        }
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
/* A symbol on the symbol panel, found the way a person finds it: 1 = it is at `area`; 0 = tap `area` first and ask
 * again (earlier pages of this tab, then later ones, then the next tab); -1 = not on the panel. UI task; the caller
 * taps with a real touch. */
static int ink_test_keyboard_symbol(const char *text, lv_area_t *area) {
    static const char *const TABS[] = {"最近", "中文", "英文", "数学", "单位", "图形", "序号"};
    static char looking[16];
    static int backward;
    if (strcmp(looking, text)) { snprintf(looking, sizeof looking, "%s", text); backward = 1; }
    lv_obj_update_layout(lv_layer_sys());
    if (ink_test_matrix_key(lv_layer_sys(), text, area)) return 1;
    lv_obj_t *open = ink_test_keyboard_label(lv_layer_sys(), NULL, true), *next = NULL; /* none: a password's one table */
    int page = 0, pages = 0;
    for (int p = 1; p < 40 && !pages; p++) for (int n = p; n < 40 && !pages; n++) {
        char label[16]; snprintf(label, sizeof label, "%d/%d", p, n);
        if (ink_test_keyboard_label(lv_layer_sys(), label, false)) { page = p; pages = n; }
    }
    if (backward && page > 1) next = ink_test_keyboard_label(lv_layer_sys(), "◀", false);
    else if ((backward = 0), page < pages) next = ink_test_keyboard_label(lv_layer_sys(), "▶", false);
    else {
        int at = -1;
        if (!open) return -1;
        for (int i = 0; i < 7; i++) if (!strcmp(lv_label_get_text(lv_obj_get_child(open, 0)), TR(TABS[i]))) at = i;
        for (int i = 1; at >= 0 && i < 7 && !next; i++) next = ink_test_keyboard_label(lv_layer_sys(), TR(TABS[(at + i) % 7]), false);
    }
    if (!next) return -1;
    lv_obj_get_coords(next, area);
    return 0;
}
#endif
