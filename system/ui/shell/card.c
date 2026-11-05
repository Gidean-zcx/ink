/* Common opaque bottom panel and full-screen interruption surface. */
#include "ink/ui_theme.h"
#include "shell_card.h"

#define CARD_HEIGHT (INK_MARGIN * 2 + INK_CIRCLE + INK_GAP_L * 2 + INK_ROW_DOUBLE * 2 + INK_PRIMARY)

void shell_card_open(shell_card_t *c) {
    c->root = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(c->root);
    lv_obj_set_size(c->root, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(c->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c->root, LV_OBJ_FLAG_CLICKABLE);
    c->panel = ink_ui_panel(c->root, CARD_HEIGHT, true);
    c->title = ink_ui_label(c->panel, "", INK_FONT_SUBTITLE, INK_WEIGHT_BOLD,
                            INK_MARGIN, INK_MARGIN, INK_CANVAS_W - 2 * INK_MARGIN);
    c->body = ink_ui_label(c->panel, "", INK_FONT_BODY, INK_WEIGHT_REGULAR,
                           INK_MARGIN, INK_MARGIN + INK_CIRCLE + INK_GAP_L, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_height(c->body, ink_ui_px(2 * INK_ROW_DOUBLE));
    lv_label_set_long_mode(c->body, LV_LABEL_LONG_WRAP);
    c->row = lv_obj_create(c->panel);
    lv_obj_remove_style_all(c->row);
    lv_obj_set_pos(c->row, ink_ui_px(INK_MARGIN), ink_ui_px(CARD_HEIGHT - INK_PRIMARY - INK_MARGIN));
    lv_obj_set_size(c->row, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN), ink_ui_px(INK_PRIMARY));
    lv_obj_set_flex_flow(c->row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(c->row, ink_ui_px(INK_GAP), 0);
    ink_ui_hint_obj(c->panel, INK_REFRESH_FLASH_UI);
}

void shell_card_full(shell_card_t *c) {
    lv_obj_set_pos(c->panel, 0, 0);
    lv_obj_set_size(c->panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_radius(c->panel, 0, 0);
    lv_obj_set_style_border_width(c->panel, 0, 0);
    lv_obj_set_pos(c->title, ink_ui_px(INK_MARGIN), ink_ui_px(INK_BLOCK_L + INK_LINE_BODY));
    lv_obj_set_width(c->title, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
    lv_obj_set_style_text_align(c->title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(c->title, ink_ui_font_px(INK_FONT_HEADING, INK_WEIGHT_BOLD), 0);
    lv_obj_set_pos(c->body, ink_ui_px(INK_MARGIN), ink_ui_px(INK_BLOCK_L + INK_LINE_BODY + INK_LINE_HEADING + INK_MARGIN));
    lv_obj_set_style_text_align(c->body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_y(c->row, ink_ui_px(INK_CANVAS_H - INK_PRIMARY - INK_MARGIN));
}

void shell_card_text(shell_card_t *c, const char *title, const char *body) {
    lv_label_set_text(c->title, title);
    lv_label_set_text(c->body, body);
    ink_ui_hint_obj(c->panel, INK_REFRESH_FLASH_UI);
}

void shell_card_clear_buttons(shell_card_t *c) { lv_obj_clean(c->row); }

void shell_card_button(shell_card_t *c, const char *label, lv_event_cb_t cb, void *user) {
    lv_obj_t *b = ink_ui_pill(c->row, label, 0, 0, INK_PRIMARY, INK_PRIMARY,
                             lv_obj_get_child_count(c->row) == 0, cb, user);
    lv_obj_set_flex_grow(b, 1);
}

void shell_card_close(shell_card_t *c) {
    ink_ui_hint_obj(c->panel, INK_REFRESH_FLASH_UI);
    lv_obj_delete(c->root);
    c->root = c->panel = c->title = c->body = c->row = NULL;
}
