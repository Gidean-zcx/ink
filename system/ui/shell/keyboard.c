/* LVGL keyboard maps feed the existing text capabilities; the service owns all edits. Pages after fcitx5-android,
 * symbol tables after Trime, nine-key readings and refresh rules after Mogo (docs/tasks/keyboard-2/SOURCE.md). */
/* ui-scale-runtime: dimension area.y1,key_height,candidates_height,candidate_band_height,keyboard_height,keyboard_height_for,make_board,height,gap,cell,width,column,kbd_scaled,lv_obj_get_content_width,pill_width Theme-derived keyboard geometry scales with the current text line. */
/* ui-scale-runtime: dimension keyboard.y1 The keyboard safe area includes the wrapped edit toolbar. */
/* ui-scale-runtime: dimension candidate_width Candidate text or grid width with the keyboard's minimum touch width. */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/sys/font.h"
#include "ink/sys/input.h"
#include "ink/sys/textinput.h"
#include "ink/sys/ui.h"
#include "ink/ui_shell.h"
#include "ink/sys/cap.h"
#include "ink/sys/i18n.h"
#include "ink/sys/settings.h"
#include "ink/sys/ui_semantics.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"
#include "keyboard_theme.h"

#define KEY_BACKSPACE '\b'
#define KEY_ENTER '\n'
#define KEY_SWITCH '\1'
#define KEY_SHIFT '\2'
#define KEY_LAYOUT '\4'
#define KEY_NUMBER '\5'
#define KEY_RETYPE '\6'
enum { KEYBOARD_FIELDS_MAX = 13 + INK_TEXT_PAGE, KEYBOARD_BUTTON_MAX = 48, KEYBOARD_MAP_MAX = KEYBOARD_BUTTON_MAX + 6 };
/* Action slots of the keyboard's own buttons (readings, symbol tabs, page row, symbol grid, long-press popup). */
enum { ACT_READING = 0, ACT_TAB = 3, ACT_NAV = 10, ACT_CAND_DELETE = 15, ACT_GRID = 16, ACT_POPUP = 46, ACT_MAX = 56 };
typedef struct { const char *cap; char args[48]; } kb_action_t;

static struct {
    lv_obj_t *root, *keys, *board, *footer, *cands, *composing, *edit, *fold, *readings, *reading[3];
    lv_obj_t *cand_btn[INK_TEXT_PAGE], *cand_label[INK_TEXT_PAGE], *cand_icon[INK_TEXT_PAGE], *more, *mic;
    lv_obj_t *previous, *cand_space, *cand_page, *cand_delete, *more_icon;
    lv_obj_t *picker, *tabs, *symbol_grid, *picker_nav, *popup;
    lv_obj_t *icons[3];
    lv_obj_t *panel, *status, *heard; /* the voice panel: where the keys were, the same size */
    int first;
    int shown;
    ink_text_cand_t cand[INK_TEXT_PAGE]; /* what the candidate buttons show */
    int session;                         /* the input they and the keys are for */
    bool symbols, shift, caps, expanded, secret_present, plain, english, password, numeric, long_pressed;
    int drawn_case, category, symbol_page, page_size, popup_letter, down_id, down_y, space_key;
    size_t reading_offset;               /* Mogo's s_syllable_offset */
    int readings_shown;
    char reading_for[48];                /* the composition the reading pages are for */
    const char *notice;                  /* "先选词或按重输": until the next key (no timer) */
    lv_timer_t *cleanup;
    unsigned candidate_changes;
    kb_action_t actions[ACT_MAX];
    const char *map[KEYBOARD_MAP_MAX];
    lv_buttonmatrix_ctrl_t control[KEYBOARD_BUTTON_MAX];
    char codes[KEYBOARD_BUTTON_MAX], legends[KEYBOARD_BUTTON_MAX][24];
    int buttons, map_size, footer_offset;
    uint32_t version, generation, revision, edit_revision, layout_revision, pressed_layout, voice_revision, pressed_voice;
    int built_for; /* 1 letters, 2 symbols, 3 digits, 4 nine-key, 5 password letters */
} K INK_PSRAM_BSS;

#include "vendor/mogo/mogo_nine.h"

static const lv_font_t *font(void) { return ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR); }

static void sync(void);
static void picker_snapshot(char *out, size_t cap);
static const char *category_name(void);
static int symbol_count(int category);
static void set_text(lv_obj_t *label, const char *text);
static const ink_call_t *enter_call;
/* Text clients have no source argument; unknown submissions keep the AI policy. */
ink_source_t ink_shell_keyboard_enter_source(void) { return enter_call ? enter_call->src : INK_SRC_AI; }

static int gap(void) { return kbd_scaled(INK_KB_GAP); }
static int key_height(bool digits) {
    (void)digits;
    return kbd_scaled(INK_KBD_KEY_HEIGHT);
}
static int candidates_height(void) {
    int height = kbd_scaled(INK_KBD_TOUCH);
    return height < INK_KBD_TOUCH ? INK_KBD_TOUCH : height;
}
static int candidate_band_height(void) {
    return LV_MAX(ink_ui_line_height(INK_FONT_LIST) + candidates_height() + 2 * gap(), 2 * INK_CONTROL + INK_GAP_XS + gap());
}
static bool editing(void) {
    size_t start, end; ink_text_selection(&start, &end);
    return ink_text_type() != INK_TEXT_PASSWORD && (start != end || (!ink_text_composing()[0] &&
        (ink_text_can_redo() || (K.root && K.edit && !lv_obj_has_flag(K.edit, LV_OBJ_FLAG_HIDDEN) && ink_text_can_undo()))));
}
static int keyboard_height_for(ink_text_type_t type) {
    /* Every layout is four key rows: letters, nine-key or the number pad over their function row. */
    int top = type == INK_TEXT_PASSWORD ? 0 : candidate_band_height();
    return top + 4 * key_height(false) + 3 * gap();
}
static int keyboard_height(void) { return keyboard_height_for(ink_text_type()); }
static int candidate_width(const char *word, bool ai) {
    int g = ink_ui_px(kbd_scaled(INK_GAP_XS));
    int w = K.expanded ? (lv_obj_get_content_width(K.cands) - 2 * g) / 3 :
        ink_ui_px(ink_ui_text_width(word, INK_FONT_LIST, INK_WEIGHT_REGULAR) + 2 * INK_GAP_XS + (ai ? INK_ICON_LIST + INK_GAP_XS : 0));
    return w < ink_ui_px(candidates_height()) ? ink_ui_px(candidates_height()) : w;
}
/* A field measures the keyboard its own input type will get, not the one still showing for the last field. */
void ink_shell_keyboard_area_for(ink_text_type_t type, lv_area_t *area) {
    int height = ink_ui_px(keyboard_height_for(type) + 2 * INK_KB_PAD_Y + 2 * INK_STROKE);
    *area = (lv_area_t){0, lv_display_get_vertical_resolution(NULL) - height,
                       lv_display_get_horizontal_resolution(NULL) - 1, lv_display_get_vertical_resolution(NULL) - 1};
}
void ink_shell_keyboard_area(lv_area_t *area) { ink_shell_keyboard_area_for(ink_text_type(), area); }

static bool may_edit(ink_call_t *c) {
    /* A touch acts on what it came down on; with the finger down the keyboard is not redrawn (sync_later), so another
     * input focused meanwhile is not the one it showed. */
    if (c->page_gen && !(c->src == INK_SRC_TOUCH ? ink_ui_touch_current(c->page_gen) && K.session == ink_text_target_session() :
                                                   c->page_gen == ink_ui_content_version())) {
        c->rc.status = INK_RC_STALE;
        return false;
    }
    if (!ink_text_focused() || ((c->src != INK_SRC_TOUCH && c->src != INK_SRC_KEY) &&
        (ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden()))) {
        c->rc.status = INK_RC_REJECTED;
        return false;
    }
    return true;
}
/* Mogo: nine-key digits are never text; what would put them there asks for a word first. */
static bool nine_composing(void) { return K.built_for == 4 && ink_text_composing()[0]; }
static void notice(const char *text) { K.notice = text; K.revision++; sync(); }
static void close_popup(void);
static void show_panel(bool symbols, bool numeric) {
    K.symbols = symbols; K.numeric = numeric; K.expanded = false; K.first = 0;
    if (symbols) { K.category = K.plain && !K.english ? 1 : 2; K.symbol_page = 0; } /* 中文, or 英文 for English */
    close_popup();
    ink_ui_content_changed();
    sync();
}
static void press_key(ink_call_t *call) {
    int64_t code = 0; ink_args_get_int(&call->args, "code", &code);
    if (!may_edit(call)) return;
    char c = (char)code;
    K.notice = NULL;
    if (nine_composing() && (c == KEY_ENTER || c == INK_TEXT_KEY_LANGUAGE || c == KEY_LAYOUT || c == KEY_SWITCH ||
                             c == KEY_NUMBER || c == ',' || c == '.' || (c == ' ' && !K.shown))) {
        notice(TR("先选词或按重输"));
        return;
    }
    size_t before = strlen(ink_text_composing());
    switch (c) {
    case KEY_BACKSPACE: ink_text_backspace(); break;
    case KEY_RETYPE: ink_text_clear_composing(); break;
    case KEY_ENTER:
        enter_call = call;
        ink_text_enter();
        enter_call = NULL;
        break;
    case KEY_SWITCH: show_panel(!K.symbols, false); return;
    case KEY_NUMBER: show_panel(false, !K.numeric); return;
    case KEY_SHIFT: /* once, locked, off: each a visible state, no long press or double tap (fcitx CapsKey, by taps) */
        if (K.caps) K.caps = false;
        else if (K.shift) { K.shift = false; K.caps = true; }
        else K.shift = true;
        K.revision++; ink_ui_content_changed(); sync();
        return;
    case ' ': /* the space takes the first candidate: the one shown, though the list changed under the finger */
        if (call->src == INK_SRC_TOUCH && ink_text_composing()[0] && !K.first && K.shown > 0) {
            ink_text_pick_shown(&K.cand[0]);
            break;
        }
        /* fall through */
    default:
        if ((K.shift || K.caps) && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        K.shift = false;
        ink_text_key(c);
        if (before && strlen(ink_text_composing()) == before && !ink_text_english() &&
            (K.built_for == 4 ? c >= '2' && c <= '9' : c >= 'a' && c <= 'z')) notice(TR("拼音过长，先选词")); /* Mogo :269-271 */
        break;
    }
    K.first = 0;
    ink_ui_cause(osal_now_us(), "text.key_to_visible_ms");
}

static void pick(ink_call_t *call) {
    int64_t index = 0; ink_args_get_int(&call->args, "index", &index);
    if (!may_edit(call)) return;
    int at = (int)index - K.first;
    K.first = 0;
    K.expanded = false;
    K.notice = NULL;
    /* A touch picks the word it was on: with the finger down the list may have changed (a suggestion came, the
     * dictionary loaded) and not been redrawn yet; by its place it would be a word the person never saw. */
    if (call->src == INK_SRC_TOUCH && at >= 0 && at < K.shown && at < INK_TEXT_PAGE) ink_text_pick_shown(&K.cand[at]);
    else ink_text_pick((int)index);
    ink_ui_cause(osal_now_us(), "text.key_to_visible_ms");
}

static void candidates(ink_call_t *call) {
    bool fold = false; ink_args_get_bool(&call->args, "fold", &fold);
    if (!may_edit(call)) return;
    ink_ui_content_changed();
    K.revision++;
    K.layout_revision++;
    if (fold) { K.expanded = false; K.first = 0; }
    else K.expanded = true;
    close_popup();
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
/* Nine-key readings (Mogo ui_keyboard.c:335-341): choose one for the first syllable, or the next group of them. */
static void reading_action(ink_call_t *call) {
    if (!may_edit(call)) return;
    K.notice = NULL;
    if (!strcmp(call->cap->id, "keyboard.readings_more")) {
        const char *out[4]; bool more; syllables(out, &more);
        K.reading_offset = more ? K.reading_offset + (size_t)K.readings_shown : 0; /* past the last: the first group */
        K.revision++;
        sync();
        return;
    }
    char text[12] = ""; ink_args_get_str(&call->args, "text", text, sizeof text);
    char digits[12];
    size_t n = strlen(text);
    bool known = !n;
    for (size_t i = 0; n && book_search_syllable(i); i++) if (!strcmp(book_search_syllable(i), text)) known = true;
    if (!known || (n && (!ui_ime_digits(text, digits, sizeof digits) || strncmp(ink_text_composing(), digits, n)))) {
        call->rc.status = INK_RC_REJECTED;
        return;
    }
    K.revision++;
    ink_text_prefer_reading(text);
}

static void pressed(lv_event_t *e) { (void)e; K.version = ink_ui_content_version(); K.generation = K.revision; K.pressed_layout = K.layout_revision; K.pressed_voice = K.voice_revision; }
static bool voice_panel_action(const char *id) { return !strcmp(id, "text.voice_finish") || !strcmp(id, "text.voice_cancel"); }
static void invoke(const char *id, const char *json) {
    ink_ui_action_spec_t action = {.id = id, .label = id, .capability = id, .args_json = json, .enabled = true};
    ink_ui_page_spec_t page = {.id = ink_ui_action_page(), .object = {"keyboard", K.revision}, .actions = &action, .action_count = 1};
    ink_ui_action_target_t target = {K.version ? K.version : ink_ui_content_version(), {"keyboard", K.version ? K.generation : K.revision}};
    if (!strcmp(id, "keyboard.candidates") || !strcmp(id, "keyboard.page")) {
        /* Voice availability changes do not change this control's meaning. */
        page.object = (ink_ui_object_ref_t){"keyboard:layout", K.layout_revision};
        target = (ink_ui_action_target_t){ink_ui_content_version(), {"keyboard:layout", K.version ? K.pressed_layout : K.layout_revision}};
    } else if (voice_panel_action(id)) {
        /* Words heard while the finger is down change the page, not what these two buttons mean: they hold until the panel goes. */
        page.object = (ink_ui_object_ref_t){"keyboard:voice", K.voice_revision};
        target = (ink_ui_action_target_t){ink_ui_content_version(), {"keyboard:voice", K.version ? K.pressed_voice : K.voice_revision}};
    }
    K.version = 0;
    ink_receipt_t rc; ink_ui_semantics_action(&page, id, &target, INK_SRC_TOUCH, &rc);
}
/* The keyboard's own buttons call the same capability the AI calls, from an action slot. */
static void on_action(lv_event_t *e) {
    kb_action_t *a = lv_event_get_user_data(e);
    if (a->cap) invoke(a->cap, a->args[0] ? a->args : NULL);
}
static kb_action_t *action_at(int slot, const char *cap, const char *args) {
    kb_action_t *a = &K.actions[slot];
    a->cap = cap;
    snprintf(a->args, sizeof a->args, "%s", args ? args : "");
    return a;
}
static void text_args(char *out, size_t cap, const char *text) { /* {"text":"…"} with the two characters JSON escapes */
    size_t at = 9;
    memcpy(out, "{\"text\":\"", at);
    for (const char *p = text; *p && at + 4 < cap; p++) { if (*p == '"' || *p == '\\') out[at++] = '\\'; out[at++] = *p; }
    memcpy(out + at, "\"}", 3);
}
/* A key-like button: paper, no frame, no pressed look (D-31: what it does is the feedback). */
static lv_obj_t *kb_button(lv_obj_t *parent, const char *label, int slot, const char *cap, const char *args, int width) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_style_bg_color(b, ink_ui_paper(), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(b, ink_ui_ink(), 0);
    lv_obj_set_size(b, width, ink_ui_px(candidates_height()));
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, font(), 0);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, label);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(b, on_action, LV_EVENT_CLICKED, action_at(slot, cap, args));
    return b;
}
static void show_alternates(lv_event_t *e);
/* fcitx AlphabetKey swipe: up by half a key on a letter types its alternate (the small mark at its top right). */
static bool alternate(int letter, int n, char out[16]);
static int letter_of(int id) {
    char c = id >= 0 && id < K.buttons ? K.codes[id] : 0;
    if ((K.built_for != 1 && K.built_for != 5) || c < 'a' || c > 'z') return -1;
    return c - 'a' + (K.shift || K.caps ? 26 : 0);
}
static bool swiped(void) {
    lv_indev_t *indev = lv_indev_active();
    if (!indev || letter_of(K.down_id) < 0 || K.long_pressed) return false;
    lv_point_t p; lv_indev_get_point(indev, &p);
    return K.down_y - p.y > ink_ui_px(key_height(false)) / 2;
}
static void act(const char *cap, const char *args) {
    /* Keys press with screen invalidation off (feedback_event); what the key does draws. */
    bool invalidation = lv_display_is_invalidation_enabled(NULL);
    int kind = K.built_for; ink_text_type_t type = ink_text_type();
    if (!invalidation) lv_display_enable_invalidation(NULL, true);
    invoke(cap, args);
    if (!invalidation && K.root && K.built_for == kind && ink_text_type() == type)
        lv_display_enable_invalidation(NULL, false);
}
static void on_key(lv_event_t *e) {
    lv_obj_t *board = lv_event_get_target_obj(e);
    uint32_t id = lv_keyboard_get_selected_button(board) + (board == K.footer ? (uint32_t)K.footer_offset : 0);
    if (K.long_pressed || swiped() || id >= (uint32_t)K.buttons || !K.codes[id]) return;
    char args[40]; snprintf(args, sizeof args, "{\"code\":%d}", (int)K.codes[id]);
    act("keyboard.key", args);
}
static void key_down(lv_event_t *e) {
    pressed(e);
    lv_obj_t *board = lv_event_get_target_obj(e);
    uint32_t id = lv_buttonmatrix_get_selected_button(board);
    K.down_id = id == LV_BUTTONMATRIX_BUTTON_NONE ? -1 : (int)id + (board == K.footer ? K.footer_offset : 0);
    K.long_pressed = false;
    lv_point_t p = {0, 0};
    if (lv_indev_active()) lv_indev_get_point(lv_indev_active(), &p);
    K.down_y = p.y;
}
static void key_up(lv_event_t *e) {
    (void)e;
    char text[16], args[48];
    if (swiped() && alternate(letter_of(K.down_id), 0, text)) { text_args(args, sizeof args, text); act("keyboard.symbol", args); }
    K.down_id = -1;
}
static void on_cand(lv_event_t *e) {
    char args[40]; snprintf(args, sizeof args, "{\"index\":%d}", K.first + (int)(intptr_t)lv_event_get_user_data(e));
    invoke("keyboard.pick", args);
}
static void on_more(lv_event_t *e) { (void)e; invoke(K.expanded ? "keyboard.page" : "keyboard.candidates", K.expanded ? "{\"direction\":1}" : "{}"); }
static void on_previous(lv_event_t *e) { (void)e; invoke("keyboard.page", "{\"direction\":-1}"); }
static void on_fold(lv_event_t *e) { (void)e; invoke("keyboard.candidates", "{\"fold\":true}"); }
static void edit_action(lv_event_t *e) {
    char args[40]; snprintf(args, sizeof args, "{\"action\":%d}", (int)(intptr_t)lv_event_get_user_data(e));
    invoke("keyboard.edit", args);
}

static int candidate_total(void) { /* the service holds at most 60: find the end in a few cached reads */
    int low = 0, high = INK_KBD_CANDIDATES;
    ink_text_cand_t c;
    while (low < high) { int mid = (low + high + 1) / 2; if (ink_text_candidates(mid - 1, &c, 1)) low = mid; else high = mid - 1; }
    return low;
}
static void readings_text(char *out, size_t cap) {
    const char *list[4]; bool more; size_t n = syllables(list, &more), used = 0;
    out[0] = 0;
    for (int i = 0; i < K.readings_shown && i < (int)n; i++) {
        int w = snprintf(out + used, cap - used, "%s%s", i ? "," : "", list[i]);
        if (w > 0 && (size_t)w < cap - used) used += (size_t)w;
    }
}
static int semantic_snapshot(char *buf, size_t cap) {
    bool secret = ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden();
    if (K.root) lv_obj_update_layout(K.root);
    size_t start, end; ink_text_selection(&start, &end);
    char selection[96], mode[320], cursor[64], undo[64], symbols[256], readings[128], composition[UI_IME_RAW_MAX * 7 + 1], shown[64];
    picker_snapshot(symbols, sizeof symbols);
    if (K.built_for == 4) composition_text(composition);
    else snprintf(composition, sizeof composition, "%.40s", ink_text_composing());
    readings_text(shown, sizeof shown);
    snprintf(readings, sizeof readings, "可见=%.60s；选中=%.8s；更多=%d", shown, ink_text_preferred_reading(),
             K.readings && K.actions[ACT_READING + 2].cap && !strcmp(K.actions[ACT_READING + 2].cap, "keyboard.readings_more"));
    snprintf(selection, sizeof selection, "选区=%zu..%zu", start, end);
    snprintf(cursor, sizeof cursor, "光标=%zu", ink_text_cursor());
    int total = candidate_total();
    int symbol_pages = K.page_size ? LV_MAX(1, (symbol_count(K.category) + K.page_size - 1) / K.page_size) : 1;
    snprintf(mode, sizeof mode, "模式=%s；输入=%s；展开=%d；候选起点=%d；更多候选=%d；候选页=%d/%d；大写=%s；分类=%s；符号页=%d/%d",
             K.built_for == 4 ? "九宫格" : K.built_for == 3 ? "数字" : K.symbols ? "符号" : K.caps ? "大写锁定" : K.shift ? "大写" : "字母",
             K.plain && !K.english ? "中文" : "英文", K.expanded, K.first, K.more && lv_obj_is_visible(K.more),
             K.first / INK_TEXT_PAGE + 1, total ? (total + INK_TEXT_PAGE - 1) / INK_TEXT_PAGE : 1,
             K.caps ? "锁定" : K.shift ? "一次" : "关", category_name(), K.symbol_page + 1, symbol_pages);
    snprintf(undo, sizeof undo, "撤销=%d；重做=%d", ink_text_can_undo(), ink_text_can_redo());
    ink_ui_field_spec_t base[] = {
        {.id = "value", .role = "textbox", .label = secret ? "密码输入；内容=******" : "内容", .value = ink_text_draft_id() ? "" : ink_text_value(), .secret = secret},
        {.id = "composing", .role = "text", .label = "组合", .value = secret ? "" : composition},
        {.id = "notice", .role = "status", .label = "提示", .value = K.notice ? K.notice : ""},
        {.id = "readings", .role = "text", .label = "读音", .value = K.built_for == 4 ? readings : ""},
        {.id = "cursor", .role = "status", .label = "光标", .value = cursor},
        {.id = "selection", .role = "status", .label = "选区", .value = selection},
        {.id = "mode", .role = "status", .label = "模式", .value = mode},
        {.id = "undo", .role = "status", .label = "撤销", .value = undo},
        {.id = "keys", .role = "text", .label = "按键", .value = K.built_for == 3 ? "删除=keyboard.key(8)；回车=keyboard.key(10)" : K.plain ?
            "符号=keyboard.key(1)；大写=keyboard.key(2)；删除=keyboard.key(8)；中英切换=keyboard.key(3)；全键/九键=keyboard.key(4)；数字=keyboard.key(5)；重输=keyboard.key(6)；回车=keyboard.key(10)" :
            "符号=keyboard.key(1)；大写=keyboard.key(2)；删除=keyboard.key(8)；数字=keyboard.key(5)；回车=keyboard.key(10)"},
        {.id = "symbols", .role = "text", .label = K.popup_letter >= 0 ? "长按备选" : "可见符号", .value = symbols},
        {.id = "voice", .role = "status", .label = "听写", .value = K.status ? lv_label_get_text(K.status) : ""},
        {.id = "heard", .role = "text", .label = "听到", .value = K.heard ? lv_label_get_text(K.heard) : ""}
    };
    static ink_ui_field_spec_t fields[KEYBOARD_FIELDS_MAX] INK_PSRAM_BSS;
    _Static_assert(sizeof base / sizeof *base + INK_TEXT_PAGE <= KEYBOARD_FIELDS_MAX, "keyboard field capacity");
    memcpy(fields, base, sizeof base);
    char candidates[INK_TEXT_PAGE][128], ids[INK_TEXT_PAGE][24];
    size_t count = secret ? 1 : ink_text_voice_state() == INK_VOICE_IDLE ? 10 : 12;
    if (!secret && K.cands && lv_obj_is_visible(K.cands)) for (int i = 0; i < INK_TEXT_PAGE; i++) if (lv_obj_is_visible(K.cand_btn[i])) {
        snprintf(ids[i], sizeof ids[i], "candidate:%d", K.first + i);
        snprintf(candidates[i], sizeof candidates[i], "候选[%d]=%.90s", K.first + i, lv_label_get_text(K.cand_label[i]));
        fields[count++] = (ink_ui_field_spec_t){.id = ids[i], .role = "button", .label = K.cand_icon[i] && lv_obj_is_visible(K.cand_icon[i]) ? "AI 接话" : "候选", .value = candidates[i]};
    }
    static const char *const CAPS[] = {"text.insert", "text.select", "text.submit", "keyboard.key", "keyboard.pick", "keyboard.candidates", "keyboard.edit", "text.voice_start", "text.voice_finish", "text.voice_cancel", "text.undo", "text.redo", "keyboard.page", "keyboard.panel", "keyboard.category", "keyboard.symbol_page", "keyboard.symbol", "keyboard.alternates", "keyboard.reading", "keyboard.readings_more"};
    ink_ui_action_spec_t actions[sizeof CAPS / sizeof *CAPS];
    bool voice = ink_text_voice_state() != INK_VOICE_IDLE;
    for (size_t i = 0; i < sizeof CAPS / sizeof *CAPS; i++) {
        actions[i] = (ink_ui_action_spec_t){.id = CAPS[i], .label = CAPS[i], .capability = CAPS[i], .enabled = !secret && (i >= 10 || (voice ? i == 8 || i == 9 : i < 7 || (i == 7 && K.mic && lv_obj_is_visible(K.mic))))};
        if (!strcmp(CAPS[i], "keyboard.candidates")) actions[i].object = (ink_ui_object_ref_t){"keyboard:layout", K.layout_revision};
        if (i >= 12) actions[i].enabled = !secret && !voice;
        if (i == 12) actions[i].enabled &= K.expanded;
        if (i == 14 || i == 15) actions[i].enabled &= K.symbols;
        if (i == 16) actions[i].enabled &= K.symbols || K.popup;
        if (i == 17) actions[i].enabled &= (K.built_for == 1 || K.built_for == 5) && !K.expanded;
        if (i >= 18) actions[i].enabled &= K.built_for == 4 && ink_text_composing()[0];
        if (voice_panel_action(CAPS[i])) actions[i].object = (ink_ui_object_ref_t){"keyboard:voice", K.voice_revision};
    }
    ink_ui_page_spec_t page = {.id = ink_ui_action_page(), .title = "键盘", .object = {"keyboard", K.revision},
        .fields = fields, .field_count = count, .actions = actions, .action_count = sizeof actions / sizeof *actions};
    return ink_ui_semantics_summary(&page, buf, cap);
}
/* Home and back put the keyboard away; the page keys turn whatever pages are open: candidates, symbols, else the text. */
static bool overlay_key(ink_key_t k, bool lng) {
    if (!lng && (k == INK_KEY_HOME || k == INK_KEY_BACK)) ink_text_blur(0);
    if (!lng && (k == INK_KEY_NEXT || k == INK_KEY_PREV))
        invoke(K.expanded ? "keyboard.page" : K.symbols ? "keyboard.symbol_page" : "text.page", k == INK_KEY_NEXT ? "{\"direction\":1}" : "{\"direction\":-1}");
    return true;
}
static const ink_ui_overlay_t OVERLAY = {.name = "Keyboard", .level = -2, .on_key = overlay_key, .snapshot = semantic_snapshot};

static void on_said(lv_event_t *e) {
    (void)e;
    invoke("text.voice_finish", NULL);
}

static void on_use_keys(lv_event_t *e) {
    (void)e;
    invoke("text.voice_cancel", NULL);
}

static lv_obj_t *new_row(lv_obj_t *parent) {
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(r, ink_ui_px(kbd_scaled(INK_GAP_XS)), 0);
    return r;
}

static const char *enter_label(void) {
    static const char *const N[] = {TR_NOOP("完成"), TR_NOOP("发送"), TR_NOOP("搜索"), TR_NOOP("下一个")};
    return TR(N[ink_text_enter_kind()]);
}

static void map_key(const char *label, char code, int width) {
    int i = K.buttons++;
    snprintf(K.legends[i], sizeof K.legends[i], "%s", label);
    K.map[K.map_size++] = K.legends[i];
    K.codes[i] = code;
    K.control[i] = (lv_buttonmatrix_ctrl_t)(width | LV_BUTTONMATRIX_CTRL_CLICK_TRIG | (code == KEY_BACKSPACE ? 0 : LV_BUTTONMATRIX_CTRL_NO_REPEAT) | (code ? 0 : LV_BUTTONMATRIX_CTRL_HIDDEN));
}
static void map_row(const char *letters, int width) {
    for (const char *p = letters; *p; p++) {
        char text[2] = {(K.shift || K.caps) && !K.symbols && *p >= 'a' && *p <= 'z' ? (char)(*p - 'a' + 'A') : *p, 0};
        const char *punctuation = ink_text_punctuation(*p);
        map_key(*p == KEY_BACKSPACE ? TR("删除") : punctuation ? punctuation : text, *p == ' ' ? 0 : *p, width);
    }
    K.map[K.map_size++] = "\n";
}
/* One width for every function key in a row: the next step up only when a label would not fit (fcitx keeps them equal). */
static bool fits(const char *label, int units, int row_units) {
    int key = (INK_CANVAS_W - 2 * INK_GAP - 2 * INK_STROKE) * units / row_units - gap();
    return ink_ui_text_width(label, INK_FONT_LIST, INK_WEIGHT_REGULAR) + 2 * INK_GAP_XS <= key;
}
static const char *layout_label(int fn) {
    const char *label = ink_setting_get("text.nine_key") ? TR("全键") : TR("九键");
    return fits(label, fn, 3 * fn + 3 * INK_KB_UNIT + INK_KB_SPACE) ? label : ink_setting_get("text.nine_key") ? "26" : "9";
}
static int function_units(void) {
    int row = 3 * INK_KB_FUNCTION + 3 * INK_KB_UNIT + INK_KB_SPACE; /* the plain footer: the narrowest */
    bool ok = fits(TR("符号"), INK_KB_FUNCTION, row) && fits(TR("九键"), INK_KB_FUNCTION, row) && fits(TR("全键"), INK_KB_FUNCTION, row) &&
              (fits(enter_label(), INK_KB_FUNCTION, row) || ink_ui_text_width(enter_label(), INK_FONT_LIST, INK_WEIGHT_REGULAR) > INK_KB_FUNCTION_WIDTH);
    return INK_KB_FUNCTION + !ok;
}
static void map_footer(void) { /* the rows so far go to the board, the rest to the footer */
    K.map[K.map_size - 1] = "";
    K.footer_offset = K.buttons;
}
static void map_install(int kind) {
    K.buttons = K.map_size = 0;
    K.space_key = -1;
    int fn = function_units();
    _Static_assert(10 + 11 + 9 + 7 <= KEYBOARD_BUTTON_MAX, "largest keyboard map");
    _Static_assert(KEYBOARD_BUTTON_MAX + 5 <= KEYBOARD_MAP_MAX, "map separators and terminator");
    int map_start = 0;
    if (kind == 3 && K.numeric && ink_text_type() != INK_TEXT_NUMBER && ink_text_type() != INK_TEXT_PHONE) {
        /* fcitx NumberKeyboard: operators beside the digits, the letters and symbols keys below */
        static const char *const rows[] = {"+123/", "-456 ", "*789\b"};
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 5; col++) {
                char k = rows[row][col], text[2] = {k, 0};
                map_key(k == ' ' ? TR("空格") : k == '\b' ? TR("删除") : text, k, col == 0 || col == 4 ? INK_KB_FUNCTION : INK_KB_UNIT);
            }
            K.map[K.map_size++] = "\n";
        }
        map_footer(); map_start = K.map_size;
        map_key("26", KEY_NUMBER, fn); map_key(",", ',', INK_KB_UNIT); map_key(TR("符号"), KEY_SWITCH, fn);
        map_key("0", '0', INK_KB_UNIT); map_key("=", '=', INK_KB_UNIT); map_key(".", '.', INK_KB_UNIT);
        map_key(enter_label(), KEY_ENTER, fn);
    } else if (kind == 3) { /* number and phone fields: only what the field takes (text_service.c) */
        bool phone = ink_text_type() == INK_TEXT_PHONE;
        const char *rows[] = {"123\b", phone ? "456+" : "456.", phone ? "789*" : "789-"};
        for (int row = 0; row < 3; row++) map_row(rows[row], INK_KB_FUNCTION);
        map_footer(); map_start = K.map_size;
        if (phone) map_key("#", '#', INK_KB_FUNCTION);
        map_key("0", '0', phone ? 2 * INK_KB_FUNCTION : 3 * INK_KB_FUNCTION);
        map_key(enter_label(), KEY_ENTER, INK_KB_FUNCTION);
    } else if (kind == 4) { /* Mogo's groups without digits, the comma first; delete, retype and 。 at the right */
        static const char *const nine[] = {"，", "ABC", "DEF", "", "GHI", "JKL", "MNO", "", "PQRS", "TUV", "WXYZ", ""};
        static const char codes[] = {',', '2', '3', KEY_BACKSPACE, '4', '5', '6', KEY_RETYPE, '7', '8', '9', '.'};
        _Static_assert(sizeof nine / sizeof *nine == sizeof codes, "nine-key map");
        for (int i = 0; i < 12; i++) {
            map_key(codes[i] == KEY_BACKSPACE ? TR("删除") : codes[i] == KEY_RETYPE ? TR("重输") : codes[i] == '.' ? "。" : nine[i], codes[i], INK_KB_FUNCTION);
            if (i % 4 == 3) K.map[K.map_size++] = "\n";
        }
        map_footer(); map_start = K.map_size;
    } else {
        map_row("qwertyuiop", INK_KB_UNIT);
        map_key(" ", 0, INK_KB_HALF);
        map_row("asdfghjkl", INK_KB_UNIT);
        K.map_size--; map_key(" ", 0, INK_KB_HALF); K.map[K.map_size++] = "\n";
        map_key(TR("大写"), KEY_SHIFT, INK_KB_FUNCTION);
        map_row("zxcvbnm", INK_KB_UNIT);
        K.map_size--; map_key(TR("删除"), KEY_BACKSPACE, INK_KB_FUNCTION); K.map[K.map_size++] = "\n";
    }
    if (kind != 3) {
        map_key(K.symbols ? TR("字母") : !strcmp(ink_lang_code(), "zh") ? TR("符号") : "123", KEY_SWITCH, fn);
        if (K.plain) {
            map_key(K.english ? TR("英") : "中", INK_TEXT_KEY_LANGUAGE, INK_KB_UNIT);
            map_key(layout_label(fn), KEY_LAYOUT, fn);
            if (kind != 4) map_key(ink_text_punctuation(',') ? ink_text_punctuation(',') : ",", ',', INK_KB_UNIT);
        }
        K.space_key = K.buttons;
        map_key(kind == 4 && ink_text_composing()[0] ? TR("选择") : TR("空格"), ' ',
                K.plain ? INK_KB_SPACE + (kind == 4 ? 2 * INK_KB_UNIT : 0) : INK_KB_SPACE + 2 * INK_KB_UNIT + INK_KB_FUNCTION);
        if (kind != 4) map_key(ink_text_punctuation('.') ? ink_text_punctuation('.') : ".", '.', INK_KB_UNIT);
        map_key(enter_label(), KEY_ENTER, fn);
    }
    K.map[K.map_size] = "";
    if (kind == 3 || kind == 4) {
        lv_keyboard_set_map(K.footer, LV_KEYBOARD_MODE_USER_2, K.map + map_start, K.control + K.footer_offset);
        lv_keyboard_set_mode(K.footer, LV_KEYBOARD_MODE_USER_2);
    }
    lv_keyboard_set_map(K.board, LV_KEYBOARD_MODE_USER_1, K.map, K.control);
    lv_keyboard_set_mode(K.board, LV_KEYBOARD_MODE_USER_1);
    K.drawn_case = K.shift | K.caps << 1;
}

/* Icons for shift, delete and a long enter label; a letter's alternate small at its top right (fcitx AltText, the
 * swipe-up hint, dark gray); shift locked is drawn inverted, a state and not a press. */
static void draw_key_icon(lv_event_t *e) {
    lv_draw_task_t *task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t *base = lv_draw_task_get_draw_dsc(task);
    if (!base || base->id1 >= (uint32_t)K.buttons) return;
    unsigned index = base->id1 + (lv_event_get_target_obj(e) == K.footer ? (unsigned)K.footer_offset : 0);
    char code = K.codes[index];
    bool locked = code == KEY_SHIFT && K.caps && !K.symbols;
    lv_draw_fill_dsc_t *fill = lv_draw_task_get_fill_dsc(task);
    if (fill && locked) fill->color = ink_ui_ink();
    lv_draw_label_dsc_t *label = lv_draw_task_get_label_dsc(task);
    if (!label) return;
    lv_area_t a; lv_draw_task_get_area(task, &a);
    if (locked) label->color = ink_ui_paper();
    char alt[16];
    if (letter_of((int)index) >= 0 && alternate(letter_of((int)index), 0, alt)) {
        lv_obj_t *key_board = lv_event_get_target_obj(e);
        lv_area_t key = ((lv_buttonmatrix_t *)key_board)->button_areas[base->id1], board; lv_obj_get_coords(key_board, &board);
        lv_area_move(&key, board.x1, board.y1);
        lv_draw_label_dsc_t small; lv_draw_label_dsc_init(&small);
        small.text = alt; small.text_local = 1;
        small.font = ink_ui_font_px(INK_FONT_NOTE, INK_WEIGHT_REGULAR);
        small.color = ink_ui_secondary();
        small.align = LV_TEXT_ALIGN_RIGHT;
        lv_area_t corner = {key.x1, key.y1 + ink_ui_px(INK_GAP_XS), key.x2 - ink_ui_px(INK_GAP_XS), key.y1 + ink_ui_px(INK_LINE_NOTE)};
        lv_draw_label(label->base.layer, &small, &corner);
    }
    bool enter_icon = code == KEY_ENTER && ink_ui_text_width(enter_label(), INK_FONT_LIST, INK_WEIGHT_REGULAR) > INK_KB_FUNCTION_WIDTH;
    if (!enter_icon && code != KEY_BACKSPACE && (code != KEY_SHIFT || K.symbols)) return;
    int size = ink_ui_px(INK_ICON_MEDIUM * key_height(false) / INK_KB_KEY);
    int x = (a.x1 + a.x2 + 1 - size) / 2, y = (a.y1 + a.y2 + 1 - size) / 2;
    lv_area_t icon_area = {x, y, x + INK_ICON_MEDIUM - 1, y + INK_ICON_MEDIUM - 1};
    lv_draw_image_dsc_t image; lv_draw_image_dsc_init(&image);
    image.src = lv_image_get_src(K.icons[enter_icon ? 2 : code == KEY_BACKSPACE]);
    image.recolor = label->color; image.recolor_opa = LV_OPA_COVER;
    image.scale_x = image.scale_y = size * LV_SCALE_NONE / INK_ICON_MEDIUM;
    label->opa = LV_OPA_TRANSP;
    lv_draw_image(label->base.layer, &image, &icon_area);
}

/* LVGL invalidates keys on every press even when their pressed style is identical. Suppress that native feedback
 * around the matrix's own handling; edits and layout changes made from on_key and key_up still draw (act). */
static bool feedback_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    return code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED ||
           code == LV_EVENT_PRESS_LOST || code == LV_EVENT_LONG_PRESSED_REPEAT;
}
static void before_feedback(lv_event_t *e) { if (feedback_event(e)) lv_display_enable_invalidation(NULL, false); }
static void after_feedback(lv_event_t *e) { if (feedback_event(e)) lv_display_enable_invalidation(NULL, true); }
static lv_obj_t *make_board(lv_obj_t *parent, int height) {
    lv_obj_t *board = lv_keyboard_create(parent);
    lv_obj_remove_event_cb(board, lv_keyboard_def_event_cb);
    lv_obj_remove_style_all(board);
    lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(board, LV_PCT(100), ink_ui_px(height));
    lv_obj_set_style_pad_row(board, ink_ui_px(gap()), 0);
    lv_obj_set_style_pad_column(board, ink_ui_px(gap()), 0);
    lv_obj_set_style_bg_opa(board, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(board, ink_ui_paper(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(board, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(board, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_font(board, font(), LV_PART_ITEMS);
    lv_obj_set_style_text_color(board, ink_ui_ink(), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(board, ink_ui_paper(), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(board, ink_ui_ink(), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_event_cb(board, before_feedback, LV_EVENT_ALL | LV_EVENT_PREPROCESS, NULL);
    lv_obj_add_event_cb(board, after_feedback, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(board, on_key, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(board, key_down, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(board, key_up, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(board, show_alternates, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_flag(board, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(board, draw_key_icon, LV_EVENT_DRAW_TASK_ADDED, NULL);
    return board;
}

#include "keyboard_picker.h"

/* Mogo ui_keyboard_idle_tick: three changes of the word strip, then a pause, then one clean redraw of it; never under
 * a finger (tap-loss-1). */
static void settle_candidates(lv_timer_t *timer) {
    if (ink_input_touch_held()) { lv_timer_reset(timer); return; }
    lv_timer_pause(timer);
    if (K.root && K.candidate_changes >= 3 && !K.expanded && !K.symbols && ink_text_voice_state() == INK_VOICE_IDLE) {
        K.candidate_changes = 0;
        ink_ui_hint_obj(K.cands, INK_REFRESH_TEXT);
        lv_obj_invalidate(K.cands);
    }
}
static int pill_width(const char *label) { /* the grid row's words fit their buttons at every text size */
    return LV_MAX(candidates_height(), ink_ui_text_width(label, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * (INK_GAP_XS + INK_STROKE));
}
static void build(int kind) {
    K.revision++;
    K.layout_revision++;
    if (!K.cleanup) K.cleanup = lv_timer_create(settle_candidates, INK_KBD_IDLE_MS, NULL);
    lv_timer_pause(K.cleanup); K.candidate_changes = 0;
    bool replacing = K.root != NULL;
    if (replacing) lv_obj_clean(K.root);
    else K.root = lv_obj_create(lv_layer_sys());
    K.edit = K.readings = K.picker = K.popup = NULL; /* gone with the clean; editing() reads K.edit */
    K.popup_letter = K.down_id = -1;
    lv_obj_remove_style_all(K.root);
    lv_obj_remove_flag(K.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_area_t area;
    ink_shell_keyboard_area(&area);
    lv_obj_set_size(K.root, LV_PCT(100), ink_ui_height() - area.y1);
    lv_obj_align(K.root, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(K.root, ink_ui_paper(), 0);
    lv_obj_set_style_bg_opa(K.root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(K.root, ink_ui_ink(), 0);
    lv_obj_set_style_border_width(K.root, ink_ui_px(INK_STROKE), 0);
    lv_obj_set_style_border_side(K.root, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_radius(K.root, ink_ui_px(INK_RADIUS_PANEL), 0);
    lv_obj_set_style_pad_hor(K.root, ink_ui_px(INK_GAP), 0);
    lv_obj_set_style_pad_ver(K.root, ink_ui_px(INK_KB_PAD_Y), 0);
    lv_obj_set_style_pad_row(K.root, ink_ui_px(INK_GAP_XS), 0);
    lv_obj_set_flex_flow(K.root, LV_FLEX_FLOW_COLUMN);
    K.built_for = kind;
    K.keys = lv_obj_create(K.root);
    lv_obj_remove_style_all(K.keys);
    lv_obj_remove_flag(K.keys, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(K.keys, LV_PCT(100), ink_ui_px(keyboard_height()));
    lv_obj_set_style_pad_row(K.keys, ink_ui_px(gap()), 0);
    lv_obj_set_flex_flow(K.keys, LV_FLEX_FLOW_COLUMN);
    K.composing = lv_label_create(K.keys);
    lv_obj_set_height(K.composing, ink_ui_px(ink_ui_line_height(INK_FONT_LIST)));
    lv_obj_set_width(K.composing, LV_PCT(100));
    lv_label_set_long_mode(K.composing, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(K.composing, font(), 0);
    lv_label_set_text(K.composing, "");
    if (K.password) lv_obj_add_flag(K.composing, LV_OBJ_FLAG_HIDDEN);
    K.edit = new_row(K.keys);
    lv_obj_set_flex_flow(K.edit, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(K.edit, ink_ui_px(INK_GAP_XS), 0);
    static const char *const EDIT[] = {"◀", "▶", TR_NOOP("复制"), TR_NOOP("剪切"), TR_NOOP("粘贴"), TR_NOOP("撤销"), TR_NOOP("重做")};
    for (unsigned i = 0; i < sizeof EDIT / sizeof *EDIT; i++) {
        lv_obj_t *b = ink_ui_pill(K.edit, TR(EDIT[i]), 0, 0,
            (INK_CANVAS_W - 2 * INK_GAP - 2 * INK_STROKE - 3 * INK_GAP_XS) / 4, INK_CONTROL,
            false, i < 5 ? edit_action : NULL, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, pressed, LV_EVENT_PRESSED, NULL);
        if (i >= 5) ink_shell_bind(b, i == 5 ? "text.undo" : "text.redo", NULL);
        ink_ui_pill_text(b, INK_FONT_NOTE, INK_WEIGHT_MEDIUM);
        lv_obj_set_style_min_width(b, ink_ui_px(ink_ui_text_width(i < 2 ? EDIT[i] : TR(EDIT[i]), INK_FONT_NOTE, INK_WEIGHT_MEDIUM) + 2 * (INK_GAP_XS + INK_STROKE)), 0);
    }
    lv_obj_add_flag(K.edit, LV_OBJ_FLAG_HIDDEN);
    K.cands = new_row(K.keys);
    lv_obj_set_style_pad_row(K.cands, ink_ui_px(kbd_scaled(INK_GAP_XS)), 0);
    lv_obj_set_flex_align(K.cands, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for (int i = 0; i < INK_TEXT_PAGE; i++) {
        K.cand_btn[i] = lv_button_create(K.cands);
        ink_ui_style_button(K.cand_btn[i]);
        lv_obj_set_style_radius(K.cand_btn[i], 0, 0);
        lv_obj_set_style_pad_all(K.cand_btn[i], ink_ui_px(INK_GAP_XS), 0);
        lv_obj_set_height(K.cand_btn[i], ink_ui_px(candidates_height()));
        lv_obj_set_flex_grow(K.cand_btn[i], 1);
        lv_obj_remove_flag(K.cand_btn[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(K.cand_btn[i], on_cand, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(K.cand_btn[i], pressed, LV_EVENT_PRESSED, NULL);
        K.cand_icon[i] = NULL;
        K.cand_label[i] = lv_label_create(K.cand_btn[i]);
        lv_obj_set_style_text_font(K.cand_label[i], font(), 0);
        lv_obj_set_width(K.cand_label[i], LV_PCT(100));
        lv_label_set_long_mode(K.cand_label[i], LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(K.cand_label[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(K.cand_label[i], "");
        lv_obj_center(K.cand_label[i]);
        lv_obj_add_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_t *space = K.cand_space = lv_obj_create(K.cands);
    lv_obj_remove_style_all(space);
    lv_obj_set_size(space, 0, 0);
    lv_obj_set_flex_grow(space, 1);
    /* fcitx ExpandedCandidateLayout, at the bottom: ◀, the page, ▶, delete, back to the keys */
    K.previous = ink_ui_pill(K.cands, "◀", 0, 0, candidates_height(), candidates_height(), false, on_previous, NULL);
    lv_obj_set_style_radius(K.previous, 0, 0);
    lv_obj_add_event_cb(K.previous, pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_flag(K.previous, LV_OBJ_FLAG_HIDDEN);
    K.cand_page = lv_label_create(K.cands);
    lv_obj_set_style_text_font(K.cand_page, font(), 0);
    lv_obj_set_flex_grow(K.cand_page, 1);
    lv_obj_set_style_text_align(K.cand_page, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(K.cand_page, "");
    lv_obj_add_flag(K.cand_page, LV_OBJ_FLAG_HIDDEN);
    K.more = lv_button_create(K.cands);
    ink_ui_style_button(K.more);
    lv_obj_set_style_radius(K.more, 0, 0);
    lv_obj_set_style_pad_all(K.more, ink_ui_px(INK_GAP_XS), 0);
    lv_obj_set_height(K.more, ink_ui_px(candidates_height()));
    lv_obj_set_width(K.more, ink_ui_px(candidates_height()));
    lv_obj_add_event_cb(K.more, on_more, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(K.more, pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_t *ml = lv_label_create(K.more);
    lv_obj_set_style_text_font(ml, ink_ui_font_px(INK_FONT_NOTE, INK_WEIGHT_REGULAR), 0);
    lv_label_set_text(ml, "▾");
    lv_obj_set_style_text_opa(ml, LV_OPA_TRANSP, 0);
    lv_obj_center(ml);
    K.more_icon = ink_ui_icon(K.more, "next", INK_ICON_MEDIUM, INK_ICON_LINE, 0, 0, false);
    lv_obj_center(K.more_icon);
    lv_obj_add_flag(K.more, LV_OBJ_FLAG_HIDDEN);
    K.cand_delete = ink_ui_pill(K.cands, TR("删除"), 0, 0, pill_width(TR("删除")), candidates_height(), false, on_action,
                                action_at(ACT_CAND_DELETE, "keyboard.key", "{\"code\":8}"));
    lv_obj_set_style_radius(K.cand_delete, 0, 0);
    lv_obj_add_event_cb(K.cand_delete, pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_flag(K.cand_delete, LV_OBJ_FLAG_HIDDEN);
    K.fold = ink_ui_pill(K.cands, TR("键盘"), 0, 0, pill_width(TR("键盘")), candidates_height(), false, on_fold, NULL);
    lv_obj_set_style_radius(K.fold, 0, 0);
    lv_obj_add_event_cb(K.fold, pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_flag(K.fold, LV_OBJ_FLAG_HIDDEN);
    /* The microphone shares the function-key size, at the right of the candidates. */
    K.mic = lv_button_create(K.cands);
    ink_ui_style_button(K.mic);
    lv_obj_set_style_bg_color(K.mic, ink_ui_ink(), 0);
    lv_obj_set_style_bg_opa(K.mic, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(K.mic, ink_ui_px(INK_GAP_XS), 0);
    lv_obj_set_height(K.mic, ink_ui_px(key_height(false)));
    lv_obj_set_width(K.mic, ink_ui_px(INK_KB_FUNCTION_WIDTH));
    lv_obj_set_style_radius(K.mic, 0, 0);
    ink_shell_bind(K.mic, "text.voice_start", NULL);
    lv_obj_t *mic = lv_label_create(K.mic);
    lv_obj_set_style_text_font(mic, font(), 0);
    lv_obj_set_style_text_color(mic, ink_ui_paper(), 0);
    lv_label_set_text(mic, "");
    lv_obj_t *mic_icon = ink_ui_icon(K.mic, "mic", INK_ICON_MEDIUM, INK_ICON_SOLID, 0, 0, true);
    int mic_size = ink_ui_px(INK_ICON_MEDIUM * key_height(false) / INK_KB_KEY);
    lv_obj_set_size(mic_icon, mic_size, mic_size);
    lv_image_set_inner_align(mic_icon, LV_IMAGE_ALIGN_STRETCH);
    lv_obj_center(mic_icon);
    lv_obj_add_flag(K.mic, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *rows = K.keys;
    if (kind == 4) { /* Mogo's reading column at the left of the groups, one key wide and one key tall each */
        rows = new_row(K.keys);
        lv_obj_set_style_pad_column(rows, ink_ui_px(gap()), 0);
        K.readings = lv_obj_create(rows);
        lv_obj_remove_style_all(K.readings);
        lv_obj_remove_flag(K.readings, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(K.readings, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(K.readings, ink_ui_px(gap()), 0);
        lv_obj_update_layout(K.root);
        int column = (lv_obj_get_content_width(K.keys) - 4 * ink_ui_px(gap())) / 5;
        lv_obj_set_size(K.readings, column, LV_SIZE_CONTENT);
        for (int i = 0; i < 3; i++) {
            K.reading[i] = kb_button(K.readings, "", ACT_READING + i, NULL, NULL, column);
            lv_obj_set_height(K.reading[i], ink_ui_px(key_height(false)));
            /* Mogo draws readings smaller than the groups (22 against 30 px, ui_keyboard.c:213): zhuang keeps one line */
            lv_obj_set_style_text_font(lv_obj_get_child(K.reading[i], 0), ink_ui_font_px(INK_FONT_NOTE, INK_WEIGHT_REGULAR), 0);
        }
        K.reading_for[0] = 1; /* nothing yet: the first sync sets the cells */
    }
    K.board = make_board(rows, kind == 3 || kind == 4 ? 3 * key_height(false) + 2 * gap() : 4 * key_height(false) + 3 * gap());
    K.footer = kind == 3 || kind == 4 ? make_board(K.keys, key_height(false)) : NULL;
    if (kind == 4) lv_obj_set_flex_grow(K.board, 1); /* beside the reading column */
    static const char *const enter_icons[] = {"check", "send", "search", "next"};
    for (int i = 0; i < 3; i++) {
        K.icons[i] = ink_ui_icon(K.board, i == 2 ? enter_icons[ink_text_enter_kind()] : i ? "del" : "shift", INK_ICON_MEDIUM, INK_ICON_LINE, 0, 0, false);
        lv_obj_add_flag(K.icons[i], LV_OBJ_FLAG_HIDDEN);
    }
    map_install(kind);
    if (kind == 2) build_picker();
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
    lv_obj_t *row = new_row(K.panel);
    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = lv_button_create(row);
        ink_ui_style_button(b);
        lv_obj_set_height(b, ink_ui_px(key_height(false)));
        lv_obj_set_flex_grow(b, 1);
        lv_obj_add_event_cb(b, i ? on_use_keys : on_said, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(b, pressed, LV_EVENT_PRESSED, NULL);
        lv_obj_t *l = lv_label_create(b);
        lv_obj_set_style_text_font(l, font(), 0);
        lv_label_set_text(l, i ? TR("用键盘") : TR("说完了"));
        lv_obj_center(l);
    }
    lv_obj_add_flag(K.panel, LV_OBJ_FLAG_HIDDEN);
    ink_ui_capture_touch(K.root);
    ink_ui_overlay_add(&OVERLAY, K.root);
    ink_ui_hint_obj(K.root, INK_REFRESH_FAST);
}

/* The voice panel over the keys, or the keys back: only the fixed area changes while it lasts. */
static void sync_voice(ink_voice_state_t vs) {
    bool on = vs != INK_VOICE_IDLE, was = !lv_obj_has_flag(K.panel, LV_OBJ_FLAG_HIDDEN);
    if (on != was) {
        K.voice_revision++; /* the voice panel's buttons are new ones */
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
    lv_label_set_text(K.status, vs == INK_VOICE_CONNECTING ? TR("正在连接，可以说了") : vs == INK_VOICE_LISTENING ? TR("请说，停一下就结束") : TR("正在整理成文字"));
    lv_label_set_text(K.heard, ink_text_voice_heard());
    ink_ui_hint_obj(K.status, INK_REFRESH_FAST);
    ink_ui_hint_obj(K.heard, INK_REFRESH_FAST);
}

/* The session changed without the person (a suggestion arrived, the dictionary loaded): with a finger on a key, the
 * keys and candidates stay as pressed until it lifts (tap-loss-1). */
static void sync_now(void *unused) { (void)unused; sync(); }
static void sync_later(void) { ink_ui_after_press(sync_now, NULL); }

static void set_text(lv_obj_t *label, const char *text) { /* an unchanged label is not drawn again */
    if (strcmp(lv_label_get_text(label), text)) { lv_label_set_text(label, text); ink_ui_hint_obj(label, INK_REFRESH_FAST); }
}
/* The reading column: two readings and 更多, or three when they are all (K5); the chosen one inverted. */
static void sync_readings(void) {
    if (strcmp(K.reading_for, ink_text_composing())) { /* Mogo :60: a new composition starts at the first group */
        snprintf(K.reading_for, sizeof K.reading_for, "%s", ink_text_composing());
        K.reading_offset = 0;
    }
    const char *list[4]; bool more; size_t n = syllables(list, &more);
    bool paging = more || K.reading_offset;
    K.readings_shown = paging && n > 2 ? 2 : (int)n;
    for (int i = 0; i < 3; i++) {
        bool reading = i < K.readings_shown, chosen = reading && !strcmp(list[i], ink_text_preferred_reading());
        char args[48] = "";
        if (reading) text_args(args, sizeof args, list[i]);
        action_at(ACT_READING + i, reading ? "keyboard.reading" : i == 2 && paging ? "keyboard.readings_more" : NULL, args);
        lv_obj_t *cell = K.reading[i], *label = lv_obj_get_child(cell, 0);
        set_text(label, reading ? list[i] : i == 2 && paging ? TR("更多") : "");
        if (lv_color_eq(lv_obj_get_style_bg_color(cell, 0), chosen ? ink_ui_paper() : ink_ui_ink())) {
            lv_obj_set_style_bg_color(cell, chosen ? ink_ui_ink() : ink_ui_paper(), 0);
            lv_obj_set_style_text_color(label, chosen ? ink_ui_paper() : ink_ui_ink(), 0);
            ink_ui_hint_obj(cell, INK_REFRESH_FAST);
        }
    }
    /* Mogo :257: the space says 选择 while there is something to choose; only that key is drawn again */
    const char *space = ink_text_composing()[0] ? TR("选择") : TR("空格");
    if (K.space_key >= K.footer_offset && strcmp(K.legends[K.space_key], space)) {
        snprintf(K.legends[K.space_key], sizeof K.legends[K.space_key], "%s", space);
        lv_area_t key = ((lv_buttonmatrix_t *)K.footer)->button_areas[K.space_key - K.footer_offset], board;
        lv_obj_get_coords(K.footer, &board);
        lv_area_move(&key, board.x1, board.y1);
        ink_ui_hint_area(&key, INK_REFRESH_FAST);
        lv_obj_invalidate_area(K.footer, &key);
    }
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
            K.symbols = K.shift = K.caps = false;
            K.numeric = K.long_pressed = false;
            K.popup = NULL; K.notice = NULL;
            K.expanded = false;
            lv_timer_pause(K.cleanup); K.candidate_changes = 0;
        }
        return;
    }
    if (K.session != ink_text_target_session()) { K.shift = K.caps = false; K.notice = NULL; } /* fcitx: case resets per field */
    K.session = ink_text_target_session();
    ink_text_type_t type = ink_text_type();
    bool secret = type == INK_TEXT_PASSWORD || ink_text_ai_hidden(), present = ink_text_value()[0] != 0;
    bool edited = !secret && K.edit_revision != ink_text_edit_revision();
    if (edited) {
        K.edit_revision = ink_text_edit_revision(); K.revision++; ink_ui_content_changed();
    }
    if (secret && present != K.secret_present) K.revision++;
    K.secret_present = present;
    if (type == INK_TEXT_NUMBER || type == INK_TEXT_PHONE) K.numeric = false;
    int kind = type == INK_TEXT_NUMBER || type == INK_TEXT_PHONE || K.numeric ? 3 : K.symbols ? 2 :
               type == INK_TEXT_PLAIN && !ink_text_english() && ink_setting_get("text.nine_key") ? 4 : type == INK_TEXT_PASSWORD ? 5 : 1;
    bool plain = type == INK_TEXT_PLAIN, english = ink_text_english();
    bool password = type == INK_TEXT_PASSWORD;
    bool language_changed = K.english != english;
    if (language_changed) K.shift = K.caps = false;
    bool rebuild = !K.root || K.built_for != kind || K.plain != plain || K.password != password;
    K.plain = plain; K.english = english; K.password = password;
    if (password) K.expanded = false;
    if (rebuild) build(kind);
    /* Keys over the AI panel while a field is focused: its controls sit where the candidate row is (the
     * microphone at the right end met its hide button). Only this pair is reordered; cards, the lock mark and
     * the control centre keep their places above. */
    lv_obj_t *panel = ink_shell_ai_panel_root();
    if (panel && lv_obj_get_parent(panel) == lv_obj_get_parent(K.root) && lv_obj_get_index(K.root) < lv_obj_get_index(panel))
        lv_obj_move_to_index(K.root, lv_obj_get_index(panel));
    else if (K.drawn_case != (K.shift | K.caps << 1) || language_changed) {
        K.layout_revision++;
        close_popup();
        map_install(kind);
        ink_ui_hint_obj(K.keys, INK_REFRESH_FAST);
    }
    ink_voice_state_t vs = ink_text_voice_state();
    sync_voice(vs);
    if (vs != INK_VOICE_IDLE) return;
    char composition[UI_IME_RAW_MAX * 7 + 1];
    if (kind == 4 && ink_text_composing()[0]) composition_text(composition);
    else snprintf(composition, sizeof composition, "%.200s", ink_text_composing()[0] ? ink_text_composing() : ink_text_voice_notice());
    set_text(K.composing, K.notice ? K.notice : composition);
    if (kind == 4) sync_readings();
    ink_text_cand_t c[INK_TEXT_PAGE + 1];
    int n = type == INK_TEXT_PLAIN ? ink_text_candidates(K.first, c, INK_TEXT_PAGE + 1) : 0;
    if (!n && K.first) {
        K.first = 0;
        n = ink_text_candidates(0, c, INK_TEXT_PAGE + 1);
    }
    bool updated = K.shown != n;
    for (int i = 0; i < INK_TEXT_PAGE; i++) {
        if (i < n) {
            K.cand[i] = c[i];
            if (strcmp(lv_label_get_text(K.cand_label[i]), c[i].word)) updated = true;
            lv_label_set_text(K.cand_label[i], c[i].word);
            if (c[i].ai && !K.cand_icon[i]) {
                K.cand_icon[i] = ink_ui_icon(K.cand_btn[i], "ai", INK_ICON_LIST, INK_ICON_SOLID, 0, 0, i == 0);
                lv_obj_move_to_index(K.cand_icon[i], 0);
                lv_obj_set_flex_flow(K.cand_btn[i], LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(K.cand_btn[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_pad_column(K.cand_btn[i], ink_ui_px(INK_GAP_XS), 0);
                lv_obj_set_flex_grow(K.cand_label[i], 1); /* a full-width label beside the icon would never settle */
            }
            if (K.cand_icon[i]) {
                if (c[i].ai) lv_obj_remove_flag(K.cand_icon[i], LV_OBJ_FLAG_HIDDEN);
                else lv_obj_add_flag(K.cand_icon[i], LV_OBJ_FLAG_HIDDEN);
            }
            bool first = i == 0 && !K.expanded;
            lv_obj_set_style_bg_color(K.cand_btn[i], first ? ink_ui_ink() : ink_ui_paper(), 0);
            lv_obj_set_style_bg_color(K.cand_btn[i], first ? ink_ui_ink() : ink_ui_paper(), LV_STATE_PRESSED);
            lv_obj_set_style_text_color(K.cand_label[i], first ? ink_ui_paper() : ink_ui_ink(), 0);
            lv_obj_set_style_border_width(K.cand_btn[i], 0, 0);
            lv_obj_remove_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
        } else lv_obj_add_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (n > 0) lv_obj_remove_flag(K.more, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(K.more, LV_OBJ_FLAG_HIDDEN);
    if (ink_text_voice_possible() && !K.expanded) lv_obj_remove_flag(K.mic, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(K.mic, LV_OBJ_FLAG_HIDDEN);
    if (K.shown != n) { K.revision++; ink_ui_content_changed(); }
    K.shown = n;
    bool show_edit = editing();
    lv_obj_set_flag(K.composing, LV_OBJ_FLAG_HIDDEN, password || show_edit);
    lv_obj_t *undo = lv_obj_get_child(K.edit, 5), *redo = lv_obj_get_child(K.edit, 6);
    if (ink_text_can_undo()) lv_obj_remove_state(undo, LV_STATE_DISABLED);
    else lv_obj_add_state(undo, LV_STATE_DISABLED);
    if (ink_text_can_redo()) lv_obj_remove_state(redo, LV_STATE_DISABLED);
    else lv_obj_add_state(redo, LV_STATE_DISABLED);
    lv_obj_set_style_text_decor(lv_obj_get_child(undo, 0), ink_text_can_undo() ? LV_TEXT_DECOR_NONE : LV_TEXT_DECOR_STRIKETHROUGH, 0);
    lv_obj_set_style_text_decor(lv_obj_get_child(redo, 0), ink_text_can_redo() ? LV_TEXT_DECOR_NONE : LV_TEXT_DECOR_STRIKETHROUGH, 0);
    if (show_edit) { lv_obj_remove_flag(K.edit, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(K.cands, LV_OBJ_FLAG_HIDDEN); }
    else {
        lv_obj_add_flag(K.edit, LV_OBJ_FLAG_HIDDEN);
        if (password) lv_obj_add_flag(K.cands, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(K.cands, LV_OBJ_FLAG_HIDDEN);
    }
    lv_area_t keyboard; ink_shell_keyboard_area(&keyboard);
    lv_obj_set_height(K.root, ink_ui_height() - keyboard.y1);
    lv_obj_set_height(K.keys, ink_ui_px(keyboard_height()));
    for (uint32_t i = 0; i < lv_obj_get_child_count(K.keys); i++) {
        lv_obj_t *row = lv_obj_get_child(K.keys, (int32_t)i);
        if (row == K.composing || row == K.cands || row == K.edit) continue;
        if (K.expanded || (kind == 2 ? row != K.picker : row == K.picker)) lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
    }
    if (kind == 2) {
        lv_obj_add_flag(K.composing, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(K.cands, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(K.edit, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_set_flex_flow(K.cands, K.expanded ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_ROW);
    lv_obj_set_height(K.cands, K.expanded ? ink_ui_px(keyboard_height() - ink_ui_line_height(INK_FONT_LIST) - gap()) :
        ink_ui_px(candidate_band_height() - ink_ui_line_height(INK_FONT_LIST) - 2 * gap()));
    lv_obj_update_layout(K.cands);
    int g = ink_ui_px(kbd_scaled(INK_GAP_XS));
    int remaining = lv_obj_get_content_width(K.cands) - ink_ui_px(candidates_height()) - g;
    if (ink_text_voice_possible()) remaining -= ink_ui_px(INK_KB_FUNCTION_WIDTH) + g;
    for (int i = 0; i < INK_TEXT_PAGE; i++) {
        lv_obj_set_flex_grow(K.cand_btn[i], !K.expanded && i == 0 ? 1 : 0);
        if (i >= n) continue;
        int width = candidate_width(c[i].word, c[i].ai);
        int height = K.expanded ? (lv_obj_get_height(K.cands) - ink_ui_px(candidates_height()) - 2 * g) / 2 : ink_ui_px(candidates_height());
        lv_obj_set_height(K.cand_btn[i], height);
        lv_obj_set_style_min_width(K.cand_btn[i], width, 0);
        lv_obj_set_width(K.cand_btn[i], width);
        if (!K.expanded && width + g > remaining) {
            lv_obj_add_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
            remaining = 0;
        } else remaining -= width + g;
    }
    lv_obj_t *grid_only[] = {K.previous, K.cand_page, K.cand_delete, K.fold};
    for (unsigned i = 0; i < sizeof grid_only / sizeof *grid_only; i++) lv_obj_set_flag(grid_only[i], LV_OBJ_FLAG_HIDDEN, !K.expanded);
    lv_obj_set_flag(K.cand_space, LV_OBJ_FLAG_HIDDEN, K.expanded);
    lv_obj_set_flag(K.previous, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK, K.expanded);
    if (K.expanded) {
        int total = candidate_total(), pages = total ? (total + INK_TEXT_PAGE - 1) / INK_TEXT_PAGE : 1;
        char page[32]; snprintf(page, sizeof page, "%d/%d", K.first / INK_TEXT_PAGE + 1, pages);
        set_text(K.cand_page, page);
        lv_obj_set_state(K.previous, LV_STATE_DISABLED, !K.first);
        lv_obj_set_state(K.more, LV_STATE_DISABLED, n <= INK_TEXT_PAGE);
    } else lv_obj_remove_state(K.more, LV_STATE_DISABLED);
    lv_label_set_text(lv_obj_get_child(K.more, 0), K.expanded ? "▶" : "▾");
    lv_image_set_rotation(K.more_icon, K.expanded ? 0 : 900);
    ink_ui_hint_obj(K.cands, INK_REFRESH_FAST);
    ink_ui_hint_obj(K.composing, INK_REFRESH_FAST);
    if (updated && n > 0 && !K.expanded && !rebuild) {
        K.candidate_changes++;
        lv_timer_reset(K.cleanup); lv_timer_resume(K.cleanup);
    }
}

int ink_shell_keyboard_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "keyboard.key", .title = "按输入法的键（1 符号、2 大写、5 数字、6 重输、8 删除、10 回车，其余是 ASCII）", .input = "{\"type\":\"object\",\"properties\":{\"code\":{\"type\":\"integer\",\"description\":\"1 符号面板；2 大写（一次、锁定、关）；3 中英切换（仅普通输入框，先提交拼音）；4 全键/九键（按成员记住）；5 数字键盘；6 重输（清掉拼音）；8 删除；10 回车；其余 ASCII，标点随中英模式；九键有拼音时回车、切换和标点先要选词\",\"minimum\":1,\"maximum\":127}},\"required\":[\"code\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = press_key},
        {.id = "keyboard.pick", .title = "选输入法候选（从 0 开始）", .input = "{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"index\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = pick},
        {.id = "keyboard.candidates", .title = "展开候选，或 fold 收回键盘", .input = "{\"type\":\"object\",\"properties\":{\"fold\":{\"type\":\"boolean\"}}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = candidates},
        {.id = "keyboard.edit", .title = "编辑选中内容：0 左、1 右、2 复制、3 剪切、4 粘贴、5 撤销", .input = "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":5}},\"required\":[\"action\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = edit},
        {.id = "keyboard.page", .title = "展开的候选翻页", .input = "{\"type\":\"object\",\"properties\":{\"direction\":{\"type\":\"integer\",\"enum\":[-1,1]}},\"required\":[\"direction\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = page_candidates},
        {.id = "keyboard.panel", .title = "切换键盘面板：字母、符号、数字", .input = "{\"type\":\"object\",\"properties\":{\"panel\":{\"type\":\"string\",\"enum\":[\"letters\",\"symbols\",\"number\"]}},\"required\":[\"panel\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = panel_action},
        {.id = "keyboard.category", .title = "符号分类：0 最近、1 中文、2 英文、3 数学、4 单位、5 图形、6 序号", .input = "{\"type\":\"object\",\"properties\":{\"category\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":6}},\"required\":[\"category\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = panel_action},
        {.id = "keyboard.symbol_page", .title = "符号翻页", .input = "{\"type\":\"object\",\"properties\":{\"direction\":{\"type\":\"integer\",\"enum\":[-1,1]}},\"required\":[\"direction\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = panel_action},
        {.id = "keyboard.symbol", .title = "打出一个符号（符号面板、长按备选和上滑走这里，最多 4 个字）", .input = "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":4}},\"required\":[\"text\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = insert_symbol},
        {.id = "keyboard.alternates", .title = "展开字母的长按备选，code 为字母的 ASCII，0 收起", .input = "{\"type\":\"object\",\"properties\":{\"code\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":122}},\"required\":[\"code\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = alternate_action},
        {.id = "keyboard.reading", .title = "九键：第一个音节只要这个读音（如 zhong），空串取消", .input = "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\",\"maxLength\":8}},\"required\":[\"text\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = reading_action},
        {.id = "keyboard.readings_more", .title = "九键：下一组读音，到头回第一组", .input = "{\"type\":\"object\",\"properties\":{}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = reading_action}
    };
    static const ink_setting_def_t recent[] = {
        {.id = "keyboard.recent0", .title = TR_NOOP("最近符号一"), .type = INK_SET_INT, .max = (1LL << 40) - 1, .per_member = true, .child_may_change = true},
        {.id = "keyboard.recent1", .title = TR_NOOP("最近符号二"), .type = INK_SET_INT, .max = (1LL << 40) - 1, .per_member = true, .child_may_change = true}
    };
    if (ink_settings_define(recent, sizeof recent / sizeof *recent)) return -1;
    if (ink_cap_register(caps, sizeof caps / sizeof *caps) != 0) return -1;
    ink_text_set_presenter(sync_later);
    return 0;
}
