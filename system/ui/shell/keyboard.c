/*
 * The keyboard panel (placeholder look until the system UI, D-64): a system panel at the bottom of the screen, under the
 * cards, over the page. It shows what the input session says and feeds in what is pressed; the page above it stays usable.
 * What changes is driven with the fast waveform (foundation §4.8).
 */
/* ui-scale-runtime: dimension area.y1 Theme-derived keyboard rectangle in display pixels. */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/sys/font.h"
#include "ink/sys/textinput.h"
#include "ink/sys/ui.h"
#include "ink/ui_shell.h"
#include "ink/sys/cap.h"
#include "ink/sys/i18n.h"
#include "ink/sys/ui_semantics.h"

#define KEY_BACKSPACE '\b'
#define KEY_ENTER '\n'
#define KEY_SWITCH '\1'
#define KEY_SHIFT '\2'
enum { KEYBOARD_FIELDS_MAX = 8 + INK_TEXT_PAGE };

static struct {
    lv_obj_t *root, *keys, *cands, *composing, *edit, *fold;
    lv_obj_t *cand_btn[INK_TEXT_PAGE], *cand_label[INK_TEXT_PAGE], *more, *mic;
    lv_obj_t *panel, *status, *heard; /* the voice panel: where the keys were, the same size */
    int first;
    int shown;
    bool symbols, shift, expanded;
    uint32_t version, generation;
    int built_for; /* 1 letters, 2 symbols, 3 digits */
} K;

static const lv_font_t *font(void) { return ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR); }

static void sync(void);

void ink_shell_keyboard_area(lv_area_t *area) {
    int height = ink_ui_px(ink_ui_line_height(INK_FONT_LIST) + INK_CONTROL + 4 * INK_KEY +
                           5 * INK_GAP_XS + 2 * INK_GAP_S + 2 * INK_STROKE);
    *area = (lv_area_t){0, lv_display_get_vertical_resolution(NULL) - height,
                       lv_display_get_horizontal_resolution(NULL) - 1, lv_display_get_vertical_resolution(NULL) - 1};
}

static bool may_edit(ink_call_t *c) {
    if (c->page_gen && c->page_gen != ink_ui_content_version()) { c->rc.status = INK_RC_STALE; return false; }
    if (!ink_text_focused() || ((c->src != INK_SRC_TOUCH && c->src != INK_SRC_KEY) &&
        (ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden()))) {
        c->rc.status = INK_RC_REJECTED;
        return false;
    }
    return true;
}
static void press_key(ink_call_t *call) {
    int64_t code = 0; ink_args_get_int(&call->args, "code", &code);
    if (!may_edit(call)) return;
    char c = (char)code;
    switch (c) {
    case KEY_BACKSPACE: ink_text_backspace(); break;
    case KEY_ENTER: ink_text_enter(); break;
    case KEY_SWITCH:
        K.symbols = !K.symbols;
        ink_ui_content_changed();
        sync();
        return;
    case KEY_SHIFT: K.shift = !K.shift; ink_ui_content_changed(); return;
    default:
        if (K.shift && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        K.shift = false;
        ink_text_key(c);
        break;
    }
    K.first = 0;
    ink_ui_cause(osal_now_us(), "text.key_to_visible_ms");
}

static void pick(ink_call_t *call) {
    int64_t index = 0; ink_args_get_int(&call->args, "index", &index);
    if (!may_edit(call)) return;
    K.first = 0;
    K.expanded = false;
    ink_text_pick((int)index);
    ink_ui_cause(osal_now_us(), "text.key_to_visible_ms");
}

static void candidates(ink_call_t *call) {
    bool fold = false; ink_args_get_bool(&call->args, "fold", &fold);
    if (!may_edit(call)) return;
    ink_ui_content_changed();
    if (fold) { K.expanded = false; K.first = 0; }
    else if (K.expanded) K.first += INK_TEXT_PAGE;
    else K.expanded = true;
    sync();
}
static void edit(ink_call_t *call) {
    int64_t value = 0; ink_args_get_int(&call->args, "action", &value);
    if (!may_edit(call)) return;
    int action = (int)value;
    if (action < 2) { ink_text_move(action ? 1 : -1, true); return; }
    if (action == 5) { ink_text_undo(); return; }
    char *text = osal_malloc(INK_CLIP_MAX + 1, OSAL_MEM_EXTERNAL);
    if (!text) return;
    if (action == 4) { if (ink_clip_get(text, INK_CLIP_MAX + 1, call->src, NULL) > 0) ink_text_commit_utf8(text); }
    else if (ink_text_selected(text, INK_CLIP_MAX + 1) > 0) {
        ink_clip_set(text, false, call->src);
        if (action == 3) ink_text_backspace();
    }
    osal_free(text);
}

static void pressed(lv_event_t *e) { (void)e; K.version = ink_ui_content_version(); K.generation = ink_ui_generation(); }
static void invoke(const char *id, const char *json) {
    ink_ui_action_spec_t action = {.id = id, .label = id, .capability = id, .args_json = json, .enabled = true};
    ink_ui_page_spec_t page = {.id = ink_ui_action_page(), .object = {"keyboard", ink_ui_generation()}, .actions = &action, .action_count = 1};
    ink_ui_action_target_t target = {K.version ? K.version : ink_ui_content_version(), {"keyboard", K.version ? K.generation : ink_ui_generation()}};
    K.version = 0;
    ink_receipt_t rc; ink_ui_semantics_action(&page, id, &target, INK_SRC_TOUCH, &rc);
}
static void on_key(lv_event_t *e) {
    char args[40]; snprintf(args, sizeof args, "{\"code\":%d}", (int)(intptr_t)lv_event_get_user_data(e));
    invoke("keyboard.key", args);
}
static void on_cand(lv_event_t *e) {
    char args[40]; snprintf(args, sizeof args, "{\"index\":%d}", K.first + (int)(intptr_t)lv_event_get_user_data(e));
    invoke("keyboard.pick", args);
}
static void on_more(lv_event_t *e) { (void)e; invoke("keyboard.candidates", "{}"); }
static void on_fold(lv_event_t *e) { (void)e; invoke("keyboard.candidates", "{\"fold\":true}"); }
static void edit_action(lv_event_t *e) {
    char args[40]; snprintf(args, sizeof args, "{\"action\":%d}", (int)(intptr_t)lv_event_get_user_data(e));
    invoke("keyboard.edit", args);
}

static int semantic_snapshot(char *buf, size_t cap) {
    bool secret = ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden();
    if (K.root) lv_obj_update_layout(K.root);
    size_t start, end; ink_text_selection(&start, &end);
    char selection[96], mode[96], cursor[64], undo[64];
    snprintf(selection, sizeof selection, "选区=%zu..%zu", start, end);
    snprintf(cursor, sizeof cursor, "光标=%zu", ink_text_cursor());
    snprintf(mode, sizeof mode, "模式=%s；展开=%d；候选起点=%d；更多候选=%d", K.built_for == 3 ? "数字" : K.symbols ? "符号" : K.shift ? "大写" : "字母", K.expanded, K.first, K.more && lv_obj_is_visible(K.more));
    snprintf(undo, sizeof undo, "撤销=%d；重做=%d", ink_text_can_undo(), ink_text_can_redo());
    ink_ui_field_spec_t base[] = {
        {.id = "value", .role = "textbox", .label = secret ? "密码输入；内容=******" : "内容", .value = ink_text_draft_id() ? "" : ink_text_value(), .secret = secret},
        {.id = "composing", .role = "text", .label = "组合", .value = ink_text_composing()},
        {.id = "cursor", .role = "status", .label = "光标", .value = cursor},
        {.id = "selection", .role = "status", .label = "选区", .value = selection},
        {.id = "mode", .role = "status", .label = "模式", .value = mode},
        {.id = "undo", .role = "status", .label = "撤销", .value = undo},
        {.id = "voice", .role = "status", .label = "听写", .value = K.status ? lv_label_get_text(K.status) : ""},
        {.id = "heard", .role = "text", .label = "听到", .value = K.heard ? lv_label_get_text(K.heard) : ""}
    };
    static ink_ui_field_spec_t fields[KEYBOARD_FIELDS_MAX] INK_PSRAM_BSS;
    _Static_assert(sizeof base / sizeof *base + INK_TEXT_PAGE <= KEYBOARD_FIELDS_MAX, "keyboard field capacity");
    memcpy(fields, base, sizeof base);
    char candidates[INK_TEXT_PAGE][128], ids[INK_TEXT_PAGE][24];
    size_t count = secret ? 1 : ink_text_voice_state() == INK_VOICE_IDLE ? 6 : 8;
    if (!secret && K.cands && lv_obj_is_visible(K.cands)) for (int i = 0; i < INK_TEXT_PAGE; i++) if (lv_obj_is_visible(K.cand_btn[i])) {
        snprintf(ids[i], sizeof ids[i], "candidate:%d", K.first + i);
        snprintf(candidates[i], sizeof candidates[i], "候选[%d]=%.90s", K.first + i, lv_label_get_text(K.cand_label[i]));
        fields[count++] = (ink_ui_field_spec_t){.id = ids[i], .role = "button", .label = "候选", .value = candidates[i]};
    }
    static const char *const CAPS[] = {"text.insert", "text.select", "text.submit", "keyboard.key", "keyboard.pick", "keyboard.candidates", "keyboard.edit", "text.voice_start", "text.voice_finish", "text.voice_cancel", "text.undo", "text.redo"};
    ink_ui_action_spec_t actions[sizeof CAPS / sizeof *CAPS];
    bool voice = ink_text_voice_state() != INK_VOICE_IDLE;
    for (size_t i = 0; i < sizeof CAPS / sizeof *CAPS; i++)
        actions[i] = (ink_ui_action_spec_t){.id = CAPS[i], .label = CAPS[i], .capability = CAPS[i], .enabled = !secret && (i >= 10 || (voice ? i == 8 || i == 9 : i < 7 || (i == 7 && K.mic && lv_obj_is_visible(K.mic))))};
    ink_ui_page_spec_t page = {.id = ink_ui_action_page(), .title = "键盘", .object = {"keyboard", ink_ui_generation()},
        .fields = fields, .field_count = count, .actions = actions, .action_count = sizeof actions / sizeof *actions};
    return ink_ui_semantics_summary(&page, buf, cap);
}
static bool overlay_key(ink_key_t k, bool lng) {
    if (!lng && (k == INK_KEY_HOME || k == INK_KEY_BACK)) ink_text_blur(0);
    if (!lng && (k == INK_KEY_NEXT || k == INK_KEY_PREV)) invoke("text.page", k == INK_KEY_NEXT ? "{\"direction\":1}" : "{\"direction\":-1}");
    return true;
}
static const ink_ui_overlay_t OVERLAY = {.name = "Keyboard", .level = -2, .on_key = overlay_key, .snapshot = semantic_snapshot};

static lv_obj_t *key(lv_obj_t *row, const char *label, char code, int grow) {
    lv_obj_t *b = lv_button_create(row);
    ink_ui_style_button(b);
    lv_obj_set_style_border_width(b, ink_ui_px(INK_STROKE), 0);
    lv_obj_set_style_pad_all(b, ink_ui_px(INK_GAP_XS), 0);
    lv_obj_set_height(b, ink_ui_px(INK_KEY));
    lv_obj_set_flex_grow(b, (uint8_t)grow);
    lv_obj_add_event_cb(b, on_key, LV_EVENT_CLICKED, (void *)(intptr_t)code);
    lv_obj_add_event_cb(b, pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, font(), 0);
    lv_label_set_text(l, label);
    lv_obj_center(l);
    return b;
}

static void on_said(lv_event_t *e) {
    (void)e;
    invoke("text.voice_finish", NULL);
}

static void on_use_keys(lv_event_t *e) {
    (void)e;
    invoke("text.voice_cancel", NULL);
}

static lv_obj_t *new_row(void) {
    lv_obj_t *r = lv_obj_create(K.keys);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(r, ink_ui_px(INK_GAP_XS), 0);
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
    if (K.root) { ink_ui_overlay_remove(&OVERLAY); lv_obj_delete(K.root); }
    K.root = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(K.root);
    lv_obj_remove_flag(K.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_area_t area;
    ink_shell_keyboard_area(&area);
    lv_obj_set_size(K.root, LV_PCT(100), ink_ui_height() - area.y1);
    lv_obj_align(K.root, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(K.root, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(K.root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(K.root, lv_color_black(), 0);
    lv_obj_set_style_border_width(K.root, ink_ui_px(INK_STROKE), 0);
    lv_obj_set_style_border_side(K.root, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_radius(K.root, ink_ui_px(INK_RADIUS_PANEL), 0);
    lv_obj_set_style_pad_hor(K.root, ink_ui_px(INK_GAP), 0);
    lv_obj_set_style_pad_ver(K.root, ink_ui_px(INK_GAP_S), 0);
    lv_obj_set_style_pad_row(K.root, ink_ui_px(INK_GAP_XS), 0);
    lv_obj_set_flex_flow(K.root, LV_FLEX_FLOW_COLUMN);
    K.built_for = kind;
    K.keys = lv_obj_create(K.root);
    lv_obj_remove_style_all(K.keys);
    lv_obj_set_size(K.keys, LV_PCT(100), ink_ui_px(ink_ui_line_height(INK_FONT_LIST) + INK_CONTROL + 4 * INK_KEY + 5 * INK_GAP_XS));
    lv_obj_set_style_pad_row(K.keys, ink_ui_px(INK_GAP_XS), 0);
    lv_obj_set_flex_flow(K.keys, LV_FLEX_FLOW_COLUMN);
    K.composing = lv_label_create(K.keys);
    lv_obj_set_height(K.composing, ink_ui_px(ink_ui_line_height(INK_FONT_LIST)));
    lv_obj_set_style_text_font(K.composing, font(), 0);
    lv_label_set_text(K.composing, "");
    K.edit = new_row();
    static const char *const EDIT[] = {"◀", "▶", TR_NOOP("复制"), TR_NOOP("剪切"), TR_NOOP("粘贴"), TR_NOOP("撤销"), TR_NOOP("重做")};
    for (unsigned i = 0; i < sizeof EDIT / sizeof *EDIT; i++) {
        lv_obj_t *b = ink_ui_pill(K.edit, TR(EDIT[i]), 0, 0, 0, INK_CONTROL, false, i < 5 ? edit_action : NULL, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, pressed, LV_EVENT_PRESSED, NULL);
        if (i >= 5) ink_shell_bind(b, i == 5 ? "text.undo" : "text.redo", NULL);
        ink_ui_pill_text(b, INK_FONT_NOTE, INK_WEIGHT_MEDIUM);
        lv_obj_set_style_min_width(b, ink_ui_px(ink_ui_text_width(TR(EDIT[i]), INK_FONT_NOTE, INK_WEIGHT_MEDIUM) + 2 * (INK_GAP_XS + INK_STROKE)), 0);
        lv_obj_set_flex_grow(b, i < 2 ? 0 : 1);
    }
    lv_obj_add_flag(K.edit, LV_OBJ_FLAG_HIDDEN);
    K.cands = new_row();
    for (int i = 0; i < INK_TEXT_PAGE; i++) {
        K.cand_btn[i] = lv_button_create(K.cands);
        ink_ui_style_button(K.cand_btn[i]);
        lv_obj_set_style_pad_all(K.cand_btn[i], ink_ui_px(INK_GAP_XS), 0);
        lv_obj_set_height(K.cand_btn[i], ink_ui_px(INK_CONTROL));
        lv_obj_set_flex_grow(K.cand_btn[i], 1);
        lv_obj_add_event_cb(K.cand_btn[i], on_cand, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(K.cand_btn[i], pressed, LV_EVENT_PRESSED, NULL);
        K.cand_label[i] = lv_label_create(K.cand_btn[i]);
        lv_obj_set_style_text_font(K.cand_label[i], font(), 0);
        lv_label_set_text(K.cand_label[i], "");
        lv_obj_center(K.cand_label[i]);
        lv_obj_add_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
    }
    K.more = lv_button_create(K.cands);
    ink_ui_style_button(K.more);
    lv_obj_set_style_pad_all(K.more, ink_ui_px(INK_GAP_XS), 0);
    lv_obj_set_height(K.more, ink_ui_px(INK_CONTROL));
    lv_obj_set_width(K.more, ink_ui_px(INK_CONTROL));
    lv_obj_add_event_cb(K.more, on_more, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(K.more, pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_t *ml = lv_label_create(K.more);
    lv_obj_set_style_text_font(ml, ink_ui_font_px(INK_FONT_NOTE, INK_WEIGHT_REGULAR), 0);
    lv_label_set_text(ml, "更多");
    lv_obj_center(ml);
    lv_obj_add_flag(K.more, LV_OBJ_FLAG_HIDDEN);
    K.fold = ink_ui_pill(K.cands, "键盘", 0, 0, INK_PRIMARY, INK_CONTROL, false, on_fold, NULL);
    lv_obj_add_event_cb(K.fold, pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_flag(K.fold, LV_OBJ_FLAG_HIDDEN);
    /* the microphone: black, at the end of the candidates (the system UI's own look comes later) */
    K.mic = lv_button_create(K.cands);
    ink_ui_style_button(K.mic);
    lv_obj_set_style_bg_color(K.mic, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(K.mic, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(K.mic, ink_ui_px(INK_GAP_XS), 0);
    lv_obj_set_height(K.mic, ink_ui_px(INK_CONTROL));
    lv_obj_set_width(K.mic, ink_ui_px(INK_PRIMARY));
    lv_obj_set_style_radius(K.mic, LV_RADIUS_CIRCLE, 0);
    ink_shell_bind(K.mic, "text.voice_start", NULL);
    lv_obj_t *mic = lv_label_create(K.mic);
    lv_obj_set_style_text_font(mic, font(), 0);
    lv_obj_set_style_text_color(mic, lv_color_white(), 0);
    lv_label_set_text(mic, "");
    lv_obj_t *mic_icon = ink_ui_icon(K.mic, "mic", INK_ICON_LIST, INK_ICON_SOLID, 0, 0, true);
    lv_obj_center(mic_icon);
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
            for (const char *p = ".,?!=_%#"; *p; p++) {
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
    lv_obj_set_style_pad_row(K.panel, ink_dp(INK_GAP_XS), 0);
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
    lv_obj_set_style_pad_column(row, ink_ui_px(INK_GAP_XS), 0);
    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = lv_button_create(row);
        ink_ui_style_button(b);
        lv_obj_set_height(b, ink_ui_px(INK_KEY));
        lv_obj_set_flex_grow(b, 1);
        lv_obj_add_event_cb(b, i ? on_use_keys : on_said, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(b, pressed, LV_EVENT_PRESSED, NULL);
        lv_obj_t *l = lv_label_create(b);
        lv_obj_set_style_text_font(l, font(), 0);
        lv_label_set_text(l, i ? "用键盘" : "说完了");
        lv_obj_center(l);
    }
    lv_obj_add_flag(K.panel, LV_OBJ_FLAG_HIDDEN);
    ink_ui_capture_touch(K.root);
    ink_ui_overlay_add(&OVERLAY, K.root);
    ink_ui_hint_obj(K.root, INK_REFRESH_FLASH_UI);
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
    lv_label_set_text(K.status, vs == INK_VOICE_CONNECTING ? "正在连接，可以说了" : vs == INK_VOICE_LISTENING ? "请说，停一下就结束" : "正在整理成文字");
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
            ink_ui_overlay_remove(&OVERLAY);
            lv_obj_delete(K.root);
            K.root = NULL;
            K.first = 0;
            K.symbols = K.shift = false;
            K.expanded = false;
        }
        return;
    }
    ink_text_type_t type = ink_text_type();
    if (type != INK_TEXT_PASSWORD && !ink_text_ai_hidden()) ink_ui_content_changed();
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
            lv_obj_set_style_bg_color(K.cand_btn[i], i == 0 ? ink_ui_ink() : ink_ui_paper(), 0);
            lv_obj_set_style_text_color(K.cand_label[i], i == 0 ? ink_ui_paper() : ink_ui_ink(), 0);
            lv_obj_set_style_border_width(K.cand_btn[i], 0, 0);
            lv_obj_remove_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
        } else lv_obj_add_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (n > 0) lv_obj_remove_flag(K.more, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(K.more, LV_OBJ_FLAG_HIDDEN);
    if (ink_text_voice_possible()) lv_obj_remove_flag(K.mic, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(K.mic, LV_OBJ_FLAG_HIDDEN);
    K.shown = n;
    size_t start, end;
    ink_text_selection(&start, &end);
    bool editing = type != INK_TEXT_PASSWORD && (start != end || (!ink_text_composing()[0] &&
        (ink_text_can_redo() || (!lv_obj_has_flag(K.edit, LV_OBJ_FLAG_HIDDEN) && ink_text_can_undo()))));
    lv_obj_t *undo = lv_obj_get_child(K.edit, 5), *redo = lv_obj_get_child(K.edit, 6);
    if (ink_text_can_undo()) lv_obj_remove_state(undo, LV_STATE_DISABLED);
    else lv_obj_add_state(undo, LV_STATE_DISABLED);
    if (ink_text_can_redo()) lv_obj_remove_state(redo, LV_STATE_DISABLED);
    else lv_obj_add_state(redo, LV_STATE_DISABLED);
    lv_obj_set_style_text_decor(lv_obj_get_child(undo, 0), ink_text_can_undo() ? LV_TEXT_DECOR_NONE : LV_TEXT_DECOR_STRIKETHROUGH, 0);
    lv_obj_set_style_text_decor(lv_obj_get_child(redo, 0), ink_text_can_redo() ? LV_TEXT_DECOR_NONE : LV_TEXT_DECOR_STRIKETHROUGH, 0);
    if (editing) { lv_obj_remove_flag(K.edit, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(K.cands, LV_OBJ_FLAG_HIDDEN); }
    else { lv_obj_add_flag(K.edit, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(K.cands, LV_OBJ_FLAG_HIDDEN); }
    for (uint32_t i = 0; i < lv_obj_get_child_count(K.keys); i++) {
        lv_obj_t *row = lv_obj_get_child(K.keys, (int32_t)i);
        if (row == K.composing || row == K.cands || row == K.edit) continue;
        if (K.expanded) lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_flex_flow(K.cands, K.expanded ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_ROW);
    lv_obj_set_height(K.cands, K.expanded ? LV_SIZE_CONTENT : ink_ui_px(INK_CONTROL));
    lv_obj_update_layout(K.cands);
    int gap = ink_ui_px(INK_GAP_XS);
    int remaining = lv_obj_get_content_width(K.cands) - ink_ui_px(INK_CONTROL);
    if (ink_text_voice_possible()) remaining -= ink_ui_px(INK_PRIMARY) + gap;
    for (int i = 0; i < INK_TEXT_PAGE; i++) {
        lv_obj_set_flex_grow(K.cand_btn[i], !K.expanded && i == 0 ? 1 : 0);
        if (i >= n) continue;
        int width = ink_ui_px(ink_ui_text_width(c[i].word, INK_FONT_LIST, INK_WEIGHT_REGULAR) + 2 * INK_GAP_XS);
        lv_obj_set_style_min_width(K.cand_btn[i], width, 0);
        lv_obj_set_width(K.cand_btn[i], width);
        if (!K.expanded && width + gap > remaining) {
            lv_obj_add_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
            remaining = 0;
        } else remaining -= width + gap;
    }
    if (K.expanded) {
        lv_obj_remove_flag(K.fold, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(K.more, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    } else {
        lv_obj_add_flag(K.fold, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(K.more, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    }
    ink_ui_hint_obj(K.cands, INK_REFRESH_FAST);
    ink_ui_hint_obj(K.composing, INK_REFRESH_FAST);
}

int ink_shell_keyboard_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "keyboard.key", .title = "按输入法的键（1 符号、2 大写、8 删除、10 回车，其余是 ASCII）", .input = "{\"type\":\"object\",\"properties\":{\"code\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":127}},\"required\":[\"code\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = press_key},
        {.id = "keyboard.pick", .title = "选输入法候选（从 0 开始）", .input = "{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"index\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = pick},
        {.id = "keyboard.candidates", .title = "展开下一页候选，或 fold 收回键盘", .input = "{\"type\":\"object\",\"properties\":{\"fold\":{\"type\":\"boolean\"}}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = candidates},
        {.id = "keyboard.edit", .title = "编辑选中内容：0 左、1 右、2 复制、3 剪切、4 粘贴、5 撤销", .input = "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":5}},\"required\":[\"action\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = edit}
    };
    if (ink_cap_register(caps, sizeof caps / sizeof *caps) != 0) return -1;
    ink_text_set_presenter(sync);
    return 0;
}
