/* Password input is local only; buffers are erased on close and after submit. */
#include <stdio.h>
#include <string.h>

#include "ink/ui_theme.h"
#include "pinpad.h"

static struct {
    char pin[9];
    size_t n;
    lv_obj_t *dots;
    void (*done)(const char *pin);
} P;

static void show_dots(void) {
    char s[32] = "";
    for (size_t i = 0; i < P.n; i++) strcat(s, "●");
    if (P.dots) {
        lv_label_set_text(P.dots, P.n ? s : "输入密码");
        ink_ui_hint_obj(P.dots, INK_REFRESH_FAST);
    }
}
void shell_pinpad_clear(void) { memset(P.pin, 0, sizeof P.pin); P.n = 0; show_dots(); }
void shell_pinpad_say(const char *text) {
    if (!P.dots) return;
    lv_label_set_text(P.dots, text);
    ink_ui_hint_obj(P.dots, INK_REFRESH_FAST);
}
static void on_digit(lv_event_t *e) {
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    if (d == 10) { if (P.n) P.pin[--P.n] = 0; }
    else if (d == 11) { if (P.done) P.done(P.pin); return; }
    else if (P.n < sizeof P.pin - 1) { P.pin[P.n++] = (char)('0' + d); P.pin[P.n] = 0; }
    show_dots();
}
static void on_delete(lv_event_t *e) { (void)e; memset(P.pin, 0, sizeof P.pin); P.n = 0; P.dots = NULL; }

lv_obj_t *shell_pinpad(lv_obj_t *parent, void (*done)(const char *pin)) {
    lv_obj_t *box = ink_ui_box(parent, 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(box, on_delete, LV_EVENT_DELETE, NULL);
    P.done = done;
    ink_ui_circle(box, "lock", (INK_CANVAS_W - INK_CIRCLE_APP) / 2, INK_BLOCK_L, INK_CIRCLE_APP, INK_ICON_LIST, INK_SURFACE_BLACK, NULL, NULL);
    P.dots = ink_ui_label(box, "输入密码", INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN,
                           INK_BLOCK_L + INK_CIRCLE_APP + INK_MARGIN, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_align(P.dots, LV_TEXT_ALIGN_CENTER, 0);
    static const char *const LABEL[4][3] = {{"1", "2", "3"}, {"4", "5", "6"}, {"7", "8", "9"}, {"删除", "0", "确定"}};
    static const int CODE[4][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 0, 11}};
    int gap = INK_GAP_XL, w = 3 * INK_DIGIT + 2 * gap, x0 = (INK_CANVAS_W - w) / 2;
    int y0 = INK_CANVAS_H - INK_READING_MARGIN - 4 * INK_DIGIT - 3 * INK_GAP;
    for (int y = 0; y < 4; y++) for (int x = 0; x < 3; x++) {
        lv_obj_t *b = ink_ui_pill(box, LABEL[y][x], x0 + x * (INK_DIGIT + gap), y0 + y * (INK_DIGIT + INK_GAP),
                                  INK_DIGIT, INK_DIGIT, false, on_digit, (void *)(intptr_t)CODE[y][x]);
        lv_obj_t *l = lv_obj_get_child(b, 0);
        lv_obj_set_style_text_font(l, ink_ui_font_px(y < 3 || x == 1 ? INK_FONT_HEADING : INK_FONT_LIST, INK_WEIGHT_REGULAR), 0);
    }
    ink_ui_capture_touch(box);
    shell_pinpad_clear();
    return box;
}
