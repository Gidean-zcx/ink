/*
 * The keyboard panel (placeholder look until the system UI, D-64): a system panel at the bottom of the screen, under the
 * cards, over the page. It shows what the input session says and feeds in what is pressed; the page above it stays usable.
 * What changes is driven with the fast waveform (foundation §4.8).
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/sys/font.h"
#include "ink/sys/textinput.h"
#include "ink/sys/ui.h"

#define KEY_BACKSPACE '\b'
#define KEY_ENTER '\n'
#define KEY_SWITCH '\1'
#define KEY_SHIFT '\2'

static struct {
    lv_obj_t *root, *keys, *cands, *composing;
    lv_obj_t *cand_btn[INK_TEXT_PAGE], *cand_label[INK_TEXT_PAGE], *more, *mic;
    lv_obj_t *panel, *status, *heard; /* the voice panel: where the keys were, the same size */
    int first;
    int shown;
    bool symbols, shift;
    int built_for; /* 1 letters, 2 symbols, 3 digits */
} K;

static const lv_font_t *font(void) {
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    ink_font_t *cjk = ink_font_for_pt(10, pi.dpi);
    return cjk ? ink_font_lvgl(cjk) : ink_ui_font(0);
}

static void sync(void);

static void on_key(lv_event_t *e) {
    char c = (char)(intptr_t)lv_event_get_user_data(e);
    switch (c) {
    case KEY_BACKSPACE: ink_text_backspace(); break;
    case KEY_ENTER: ink_text_enter(); break;
    case KEY_SWITCH:
        K.symbols = !K.symbols;
        sync();
        return;
    case KEY_SHIFT: K.shift = !K.shift; return;
    default:
        if (K.shift && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        K.shift = false;
        ink_text_key(c);
        break;
    }
    K.first = 0;
    ink_ui_cause(osal_now_us(), "text.key_to_visible_ms");
}

static void on_cand(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    int first = K.first;
    K.first = 0;
    ink_text_pick(first + i);
    ink_ui_cause(osal_now_us(), "text.key_to_visible_ms");
}

static void on_more(lv_event_t *e) {
    (void)e;
    K.first += INK_TEXT_PAGE;
    sync();
}

static lv_obj_t *key(lv_obj_t *row, const char *label, char code, int grow) {
    lv_obj_t *b = lv_button_create(row);
    ink_ui_style_button(b);
    lv_obj_set_style_pad_all(b, ink_dp(2), 0);
    lv_obj_set_height(b, ink_dp(44));
    lv_obj_set_flex_grow(b, (uint8_t)grow);
    lv_obj_add_event_cb(b, on_key, LV_EVENT_CLICKED, (void *)(intptr_t)code);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, font(), 0);
    lv_label_set_text(l, label);
    lv_obj_center(l);
    return b;
}

static void on_mic(lv_event_t *e) {
    (void)e;
    ink_text_voice_start();
    ink_ui_cause(osal_now_us(), "text.voice_tap_to_panel_ms");
}

static void on_said(lv_event_t *e) {
    (void)e;
    ink_text_voice_finish();
}

static void on_use_keys(lv_event_t *e) {
    (void)e;
    ink_text_voice_cancel();
}

static lv_obj_t *new_row(void) {
    lv_obj_t *r = lv_obj_create(K.keys);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(r, ink_dp(2), 0);
    return r;
}

static void char_row(const char *keys) {
    lv_obj_t *r = new_row();
    for (const char *p = keys; *p; p++) {
        char s[2] = {*p, 0};
        key(r, s, *p, 1);
    }
}

static const char *enter_label(void) {
    static const char *const N[] = {"完成", "发送", "搜索", "下一个"};
    return N[ink_text_enter_kind()];
}

static void build(int kind) {
    if (K.root) lv_obj_delete(K.root);
    K.root = lv_obj_create(lv_layer_top());
    lv_obj_remove_flag(K.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(K.root, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_align(K.root, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(K.root, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(K.root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(K.root, lv_color_black(), 0);
    lv_obj_set_style_border_width(K.root, 2, 0);
    lv_obj_set_style_border_side(K.root, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_radius(K.root, 0, 0);
    lv_obj_set_style_pad_all(K.root, ink_dp(2), 0);
    lv_obj_set_style_pad_row(K.root, ink_dp(2), 0);
    lv_obj_set_flex_flow(K.root, LV_FLEX_FLOW_COLUMN);
    K.built_for = kind;
    K.keys = lv_obj_create(K.root);
    lv_obj_remove_style_all(K.keys);
    lv_obj_set_size(K.keys, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(K.keys, ink_dp(2), 0);
    lv_obj_set_flex_flow(K.keys, LV_FLEX_FLOW_COLUMN);
    K.composing = lv_label_create(K.keys);
    lv_obj_set_style_text_font(K.composing, font(), 0);
    lv_label_set_text(K.composing, "");
    K.cands = new_row();
    for (int i = 0; i < INK_TEXT_PAGE; i++) {
        K.cand_btn[i] = lv_button_create(K.cands);
        ink_ui_style_button(K.cand_btn[i]);
        lv_obj_set_style_pad_all(K.cand_btn[i], ink_dp(2), 0);
        lv_obj_set_height(K.cand_btn[i], ink_dp(40));
        lv_obj_set_flex_grow(K.cand_btn[i], 1);
        lv_obj_add_event_cb(K.cand_btn[i], on_cand, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        K.cand_label[i] = lv_label_create(K.cand_btn[i]);
        lv_obj_set_style_text_font(K.cand_label[i], font(), 0);
        lv_label_set_text(K.cand_label[i], "");
        lv_obj_center(K.cand_label[i]);
        lv_obj_add_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
    }
    K.more = lv_button_create(K.cands);
    ink_ui_style_button(K.more);
    lv_obj_set_style_pad_all(K.more, ink_dp(2), 0);
    lv_obj_set_height(K.more, ink_dp(40));
    lv_obj_add_event_cb(K.more, on_more, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ml = lv_label_create(K.more);
    lv_obj_set_style_text_font(ml, font(), 0);
    lv_label_set_text(ml, "更多");
    lv_obj_add_flag(K.more, LV_OBJ_FLAG_HIDDEN);
    /* the microphone: black, at the end of the candidates (the system UI's own look comes later) */
    K.mic = lv_button_create(K.cands);
    ink_ui_style_button(K.mic);
    lv_obj_set_style_bg_color(K.mic, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(K.mic, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(K.mic, ink_dp(2), 0);
    lv_obj_set_height(K.mic, ink_dp(40));
    lv_obj_add_event_cb(K.mic, on_mic, LV_EVENT_CLICKED, NULL);
    lv_obj_t *mic = lv_label_create(K.mic);
    lv_obj_set_style_text_font(mic, font(), 0);
    lv_obj_set_style_text_color(mic, lv_color_white(), 0);
    lv_label_set_text(mic, "话筒");
    lv_obj_add_flag(K.mic, LV_OBJ_FLAG_HIDDEN);
    if (kind == 3) {
        char_row("123");
        char_row("456");
        char_row("789");
        lv_obj_t *r = new_row();
        key(r, ink_text_type() == INK_TEXT_PHONE ? "+" : ".", ink_text_type() == INK_TEXT_PHONE ? '+' : '.', 1);
        key(r, "0", '0', 1);
        key(r, "删除", KEY_BACKSPACE, 1);
        key(r, enter_label(), KEY_ENTER, 1);
    } else {
        if (kind == 1) {
            char_row("qwertyuiop");
            char_row("asdfghjkl");
            lv_obj_t *r = new_row();
            key(r, "大写", KEY_SHIFT, 2);
            for (const char *p = "zxcvbnm"; *p; p++) {
                char s[2] = {*p, 0};
                key(r, s, *p, 1);
            }
            key(r, "删除", KEY_BACKSPACE, 2);
        } else {
            char_row("1234567890");
            char_row("-/:;()@&\"'");
            lv_obj_t *r = new_row();
            for (const char *p = ".,?!"; *p; p++) {
                char s[2] = {*p, 0};
                key(r, s, *p, 1);
            }
            key(r, "删除", KEY_BACKSPACE, 2);
        }
        lv_obj_t *bottom = new_row();
        key(bottom, K.symbols ? "字母" : "符号", KEY_SWITCH, 2);
        key(bottom, "空格", ' ', 5);
        key(bottom, enter_label(), KEY_ENTER, 2);
    }
    K.panel = lv_obj_create(K.root);
    lv_obj_remove_style_all(K.panel);
    lv_obj_set_width(K.panel, LV_PCT(100));
    lv_obj_set_style_pad_row(K.panel, ink_dp(4), 0);
    lv_obj_set_flex_flow(K.panel, LV_FLEX_FLOW_COLUMN);
    K.status = lv_label_create(K.panel);
    lv_obj_set_style_text_font(K.status, font(), 0);
    K.heard = lv_label_create(K.panel);
    lv_obj_set_style_text_font(K.heard, font(), 0);
    lv_obj_set_width(K.heard, LV_PCT(100));
    lv_obj_set_flex_grow(K.heard, 1);
    lv_label_set_long_mode(K.heard, LV_LABEL_LONG_WRAP);
    lv_label_set_text(K.heard, "");
    lv_obj_t *row = lv_obj_create(K.panel);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, ink_dp(2), 0);
    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = lv_button_create(row);
        ink_ui_style_button(b);
        lv_obj_set_height(b, ink_dp(44));
        lv_obj_set_flex_grow(b, 1);
        lv_obj_add_event_cb(b, i ? on_use_keys : on_said, LV_EVENT_CLICKED, NULL);
        lv_obj_t *l = lv_label_create(b);
        lv_obj_set_style_text_font(l, font(), 0);
        lv_label_set_text(l, i ? "用键盘" : "说完了");
        lv_obj_center(l);
    }
    lv_obj_add_flag(K.panel, LV_OBJ_FLAG_HIDDEN);
    ink_ui_hint_obj(K.root, INK_REFRESH_FAST);
}

/* The voice panel over the keys, or the keys back: only the fixed area changes while it lasts. */
static void sync_voice(ink_voice_state_t vs) {
    bool on = vs != INK_VOICE_IDLE, was = !lv_obj_has_flag(K.panel, LV_OBJ_FLAG_HIDDEN);
    if (on != was) {
        if (on) {
            lv_obj_update_layout(K.root);
            lv_obj_set_height(K.panel, lv_obj_get_height(K.keys)); /* the same size: nothing above it moves */
            lv_obj_add_flag(K.keys, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(K.panel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(K.panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(K.keys, LV_OBJ_FLAG_HIDDEN);
        }
        ink_ui_hint_obj(K.root, INK_REFRESH_FAST);
    }
    if (!on) return;
    lv_label_set_text(K.status, vs == INK_VOICE_CONNECTING ? "正在连接，可以说了" : vs == INK_VOICE_LISTENING ? "请说，停一下就结束" : "正在整理…");
    lv_label_set_text(K.heard, ink_text_voice_heard());
    ink_ui_hint_obj(K.status, INK_REFRESH_FAST);
    ink_ui_hint_obj(K.heard, INK_REFRESH_FAST);
}

/* What the session says now. */
static void sync(void) { /* UI task */
    if (!ink_text_focused()) {
        if (K.root) {
            lv_area_t a;
            lv_obj_get_coords(K.root, &a);
            ink_ui_hint_area(&a, INK_REFRESH_TEXT); /* the page under it comes back with its gray levels */
            lv_obj_delete(K.root);
            K.root = NULL;
            K.first = 0;
            K.symbols = K.shift = false;
        }
        return;
    }
    ink_text_type_t type = ink_text_type();
    int kind = type == INK_TEXT_NUMBER || type == INK_TEXT_PHONE ? 3 : K.symbols ? 2 : 1;
    if (!K.root || K.built_for != kind) build(kind);
    ink_voice_state_t vs = ink_text_voice_state();
    sync_voice(vs);
    if (vs != INK_VOICE_IDLE) return;
    lv_label_set_text(K.composing, ink_text_composing()[0] ? ink_text_composing() : ink_text_voice_notice());
    ink_text_cand_t c[INK_TEXT_PAGE + 1];
    int n = type == INK_TEXT_PLAIN ? ink_text_candidates(K.first, c, INK_TEXT_PAGE + 1) : 0;
    if (!n && K.first) {
        K.first = 0;
        n = ink_text_candidates(0, c, INK_TEXT_PAGE + 1);
    }
    for (int i = 0; i < INK_TEXT_PAGE; i++) {
        if (i < n) {
            lv_label_set_text(K.cand_label[i], c[i].word);
            lv_obj_remove_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
        } else lv_obj_add_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (n > INK_TEXT_PAGE) lv_obj_remove_flag(K.more, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(K.more, LV_OBJ_FLAG_HIDDEN);
    if (ink_text_voice_possible()) lv_obj_remove_flag(K.mic, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(K.mic, LV_OBJ_FLAG_HIDDEN);
    K.shown = n;
    ink_ui_hint_obj(K.cands, INK_REFRESH_FAST);
    ink_ui_hint_obj(K.composing, INK_REFRESH_FAST);
}

int ink_shell_keyboard_init(void) {
    ink_text_set_presenter(sync);
    return 0;
}
