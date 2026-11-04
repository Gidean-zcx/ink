/* See pinpad.h. One at a time: there is only one PIN being asked for. */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/sys/font.h"
#include "pinpad.h"

static struct {
    char pin[9];
    size_t n;
    lv_obj_t *dots;
    void (*done)(const char *pin);
} P;

static const lv_font_t *font(void) {
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    ink_font_t *cjk = ink_font_for_pt(12, pi.dpi);
    return cjk ? ink_font_lvgl(cjk) : ink_ui_font(0);
}

static void show_dots(void) {
    char s[10] = "";
    memset(s, '*', P.n);
    s[P.n] = 0;
    if (P.dots) {
        lv_label_set_text(P.dots, P.n ? s : "请输入 PIN");
        ink_ui_hint_obj(P.dots, INK_REFRESH_FAST);
    }
}

void shell_pinpad_clear(void) {
    memset(P.pin, 0, sizeof P.pin);
    P.n = 0;
    show_dots();
}

void shell_pinpad_say(const char *text) {
    if (P.dots) {
        lv_label_set_text(P.dots, text);
        ink_ui_hint_obj(P.dots, INK_REFRESH_FAST);
    }
}

static void on_digit(lv_event_t *e) {
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    if (d == 10) { /* delete */
        if (P.n) P.pin[--P.n] = 0;
    } else if (d == 11) { /* OK */
        if (P.done) P.done(P.pin);
        return;
    } else if (P.n < sizeof P.pin - 1) {
        P.pin[P.n++] = (char)('0' + d);
        P.pin[P.n] = 0;
    }
    show_dots();
}

static void on_delete(lv_event_t *e) {
    (void)e;
    memset(P.pin, 0, sizeof P.pin);
    P.n = 0;
    P.dots = NULL;
}

lv_obj_t *shell_pinpad(lv_obj_t *parent, void (*done)(const char *pin)) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(box, ink_dp(6), 0);
    lv_obj_add_event_cb(box, on_delete, LV_EVENT_DELETE, NULL);
    P.done = done;
    P.dots = lv_label_create(box);
    lv_obj_set_style_text_font(P.dots, font(), 0);
    static const char *const LABEL[4][3] = {{"1", "2", "3"}, {"4", "5", "6"}, {"7", "8", "9"}, {"删除", "0", "确定"}};
    static const int CODE[4][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 0, 11}};
    for (int y = 0; y < 4; y++) {
        lv_obj_t *row = lv_obj_create(box);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, ink_dp(6), 0);
        for (int x = 0; x < 3; x++) {
            lv_obj_t *b = lv_button_create(row);
            ink_ui_style_button(b);
            lv_obj_set_size(b, ink_dp(72), ink_dp(52));
            lv_obj_add_event_cb(b, on_digit, LV_EVENT_CLICKED, (void *)(intptr_t)CODE[y][x]);
            lv_obj_t *l = lv_label_create(b);
            lv_obj_set_style_text_font(l, font(), 0);
            lv_label_set_text(l, LABEL[y][x]);
            lv_obj_center(l);
        }
    }
    shell_pinpad_clear();
    return box;
}
