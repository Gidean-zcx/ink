/* See shell_card.h. Nothing about what the cards mean is here. */
#include "ink/hal_display.h"
#include "ink/sys/font.h"
#include "shell_card.h"

static const lv_font_t *font(int pt) {
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    ink_font_t *cjk = ink_font_for_pt(pt, pi.dpi); /* the UI fonts are Latin only */
    return cjk ? ink_font_lvgl(cjk) : ink_ui_font(0);
}

void shell_card_open(shell_card_t *c) {
    c->root = lv_obj_create(lv_layer_sys());
    ink_ui_style_screen(c->root);
    lv_obj_set_size(c->root, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(c->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(c->root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c->root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(c->root, ink_dp(24), 0);
    lv_obj_set_style_pad_hor(c->root, ink_dp(24), 0);
    c->title = lv_label_create(c->root);
    lv_obj_set_style_text_font(c->title, font(16), 0);
    c->body = lv_label_create(c->root);
    lv_obj_set_width(c->body, LV_PCT(100));
    lv_obj_set_style_text_align(c->body, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(c->body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(c->body, font(12), 0);
    c->row = lv_obj_create(c->root);
    lv_obj_remove_style_all(c->row);
    lv_obj_set_size(c->row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(c->row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(c->row, ink_dp(24), 0);
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
}

void shell_card_text(shell_card_t *c, const char *title, const char *body) {
    lv_label_set_text(c->title, title);
    lv_label_set_text(c->body, body);
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
}

void shell_card_clear_buttons(shell_card_t *c) { lv_obj_clean(c->row); }

void shell_card_button(shell_card_t *c, const char *label, lv_event_cb_t cb, void *user) {
    lv_obj_t *b = lv_button_create(c->row);
    ink_ui_style_button(b);
    lv_obj_set_style_pad_hor(b, ink_dp(24), 0);
    lv_obj_set_style_pad_ver(b, ink_dp(14), 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, font(12), 0);
    lv_label_set_text(l, label);
}

void shell_card_close(shell_card_t *c) {
    lv_obj_delete(c->root);
    c->root = NULL;
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI); /* the page comes back in one clean refresh */
}
