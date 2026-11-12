/* LVGL keyboard maps feed the existing text capabilities; the service owns all edits. */
/* ui-scale-runtime: dimension area.y1,key_height,candidates_height,keyboard_height,keyboard_height_for,make_board,height Theme-derived keyboard geometry scales with the current text line. */
/* ui-scale-runtime: dimension candidate_width Candidate text or grid width with the keyboard's minimum touch width. */
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
#include "ink/sys/settings.h"
#include "ink/sys/ui_semantics.h"
#include "keyboard_theme.h"

#define KEY_BACKSPACE '\b'
#define KEY_ENTER '\n'
#define KEY_SWITCH '\1'
#define KEY_SHIFT '\2'
#define KEY_LAYOUT '\4'
#define KEY_NUMBER '\5'
enum { KEYBOARD_FIELDS_MAX = 11 + INK_TEXT_PAGE, KEYBOARD_BUTTON_MAX = 48, KEYBOARD_MAP_MAX = KEYBOARD_BUTTON_MAX + 6 };

static struct {
    lv_obj_t *root, *keys, *board, *footer, *cands, *composing, *edit, *fold;
    lv_obj_t *cand_btn[INK_TEXT_PAGE], *cand_label[INK_TEXT_PAGE], *more, *mic;
    lv_obj_t *cand_icon[INK_TEXT_PAGE];
    lv_obj_t *previous, *cand_space, *picker, *tabs, *recent, *symbol_grid, *picker_nav, *popup;
    lv_obj_t *icons[3];
    lv_obj_t *panel, *status, *heard; /* the voice panel: where the keys were, the same size */
    int first;
    int shown;
    bool symbols, shift, expanded, secret_present, plain, english, drawn_shift, password;
    bool numeric, suppress_release;
    int category, symbol_page;
    char alternate[8];
    const char *map[KEYBOARD_MAP_MAX];
    lv_buttonmatrix_ctrl_t control[KEYBOARD_BUTTON_MAX];
    char codes[KEYBOARD_BUTTON_MAX], legends[KEYBOARD_BUTTON_MAX][24];
    int buttons, map_size;
    uint32_t version, generation, revision, edit_revision, layout_revision, pressed_layout, voice_revision, pressed_voice;
    int built_for; /* 1 letters, 2 symbols, 3 digits */
} K INK_PSRAM_BSS;

static const lv_font_t *font(void) { return ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR); }

static void sync(void);
static void picker_snapshot(char *out, size_t cap);
static const char *category_name(void);
static const ink_call_t *enter_call;
/* Text clients have no source argument; unknown submissions keep the AI policy. */
ink_source_t ink_shell_keyboard_enter_source(void) { return enter_call ? enter_call->src : INK_SRC_AI; }

static int key_height(bool digits) {
    return ink_ui_line_height(INK_FONT_LIST) * (digits ? INK_KB_DIGIT : INK_KBD_KEY_HEIGHT) / INK_LINE_LIST;
}
static int candidates_height(void) {
    int height = ink_ui_line_height(INK_FONT_LIST) * INK_KBD_TOUCH / INK_LINE_LIST;
    return height < INK_KBD_TOUCH ? INK_KBD_TOUCH : height;
}
static int keyboard_height_for(ink_text_type_t type) {
    bool number = type == INK_TEXT_NUMBER || type == INK_TEXT_PHONE || (K.numeric && (type == INK_TEXT_PLAIN || type == INK_TEXT_URL));
    bool nine = type == INK_TEXT_PLAIN && !ink_text_english() && !K.symbols && ink_setting_get("text.nine_key");
    int top = type == INK_TEXT_PASSWORD ? 0 : ink_ui_line_height(INK_FONT_LIST) + candidates_height() + 2 * INK_KB_GAP;
    /* Nine-key pinyin is as tall as the full keyboard (phone keyboards keep one height when switching layouts):
     * three letter-height rows of digit keys and the function row. The number pad keeps its tall digits. */
    return top + (number ? 3 * key_height(true) + 2 * key_height(false) + 4 * INK_KB_GAP :
                  nine ? 3 * key_height(false) + key_height(false) + 3 * INK_KB_GAP :
                  4 * key_height(false) + 3 * INK_KB_GAP);
}
static int keyboard_height(void) { return keyboard_height_for(ink_text_type()); }
static int candidate_width(const char *word, bool ai) {
    int gap = ink_ui_px(INK_GAP_XS);
    int w = K.expanded ? (lv_obj_get_content_width(K.cands) - 2 * gap) / 3 :
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
    K.first = 0;
    switch (c) {
    case KEY_BACKSPACE: ink_text_backspace(); break;
    case KEY_ENTER:
        enter_call = call;
        ink_text_enter();
        enter_call = NULL;
        break;
    case KEY_SWITCH:
        K.numeric = false;
        K.symbols = !K.symbols;
        ink_ui_content_changed();
        sync();
        return;
    case KEY_NUMBER: K.numeric = !K.numeric; K.symbols = false; sync(); return;
    case KEY_SHIFT: K.shift = !K.shift; K.revision++; ink_ui_content_changed(); sync(); return;
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
    K.revision++;
    K.layout_revision++;
    if (fold) { K.expanded = false; K.first = 0; }
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
static void show_alternates(lv_event_t *e);
static void on_key(lv_event_t *e) {
    if (K.suppress_release) { K.suppress_release = false; return; }
    uint32_t id = lv_keyboard_get_selected_button(lv_event_get_target_obj(e));
    if (lv_event_get_target_obj(e) == K.footer) id += 9;
    if (id >= (uint32_t)K.buttons || !K.codes[id]) return;
    char args[40]; snprintf(args, sizeof args, "{\"code\":%d}", (int)K.codes[id]);
    bool invalidation = lv_display_is_invalidation_enabled(NULL);
    int kind = K.built_for; ink_text_type_t type = ink_text_type();
    if (!invalidation) lv_display_enable_invalidation(NULL, true);
    invoke("keyboard.key", args);
    if (!invalidation && K.root && K.built_for == kind && ink_text_type() == type)
        lv_display_enable_invalidation(NULL, false);
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

static int semantic_snapshot(char *buf, size_t cap) {
    bool secret = ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden();
    if (K.root) lv_obj_update_layout(K.root);
    size_t start, end; ink_text_selection(&start, &end);
    char selection[96], mode[256], cursor[64], undo[64], symbols[160];
    picker_snapshot(symbols, sizeof symbols);
    snprintf(selection, sizeof selection, "选区=%zu..%zu", start, end);
    snprintf(cursor, sizeof cursor, "光标=%zu", ink_text_cursor());
    snprintf(mode, sizeof mode, "模式=%s；输入=%s；展开=%d；候选起点=%d；更多候选=%d；候选页=%d；分类=%s；符号页=%d", K.built_for == 4 ? "九宫格" : K.built_for == 3 ? "数字" : K.symbols ? "符号" : K.shift ? "大写" : "字母", K.plain && !K.english ? "中文" : "英文", K.expanded, K.first, K.more && lv_obj_is_visible(K.more), K.first / INK_TEXT_PAGE + 1, category_name(), K.symbol_page + 1);
    snprintf(undo, sizeof undo, "撤销=%d；重做=%d", ink_text_can_undo(), ink_text_can_redo());
    ink_ui_field_spec_t base[] = {
        {.id = "value", .role = "textbox", .label = secret ? "密码输入；内容=******" : "内容", .value = ink_text_draft_id() ? "" : ink_text_value(), .secret = secret},
        {.id = "composing", .role = "text", .label = "组合", .value = ink_text_composing()},
        {.id = "cursor", .role = "status", .label = "光标", .value = cursor},
        {.id = "selection", .role = "status", .label = "选区", .value = selection},
        {.id = "mode", .role = "status", .label = "模式", .value = mode},
        {.id = "undo", .role = "status", .label = "撤销", .value = undo},
        {.id = "keys", .role = "text", .label = "按键", .value = K.built_for == 3 ? "删除=keyboard.key(8)" : K.plain ? "大写=keyboard.key(2)；删除=keyboard.key(8)；中英切换=keyboard.key(3)；全键/九键=keyboard.key(4)" : "大写=keyboard.key(2)；删除=keyboard.key(8)"},
        {.id = "symbols", .role = "text", .label = "可见符号", .value = symbols},
        {.id = "voice", .role = "status", .label = "听写", .value = K.status ? lv_label_get_text(K.status) : ""},
        {.id = "heard", .role = "text", .label = "听到", .value = K.heard ? lv_label_get_text(K.heard) : ""}
    };
    static ink_ui_field_spec_t fields[KEYBOARD_FIELDS_MAX] INK_PSRAM_BSS;
    _Static_assert(sizeof base / sizeof *base + INK_TEXT_PAGE <= KEYBOARD_FIELDS_MAX, "keyboard field capacity");
    memcpy(fields, base, sizeof base);
    char candidates[INK_TEXT_PAGE][128], ids[INK_TEXT_PAGE][24];
    size_t count = secret ? 1 : ink_text_voice_state() == INK_VOICE_IDLE ? 8 : 10;
    if (!secret && K.cands && lv_obj_is_visible(K.cands)) for (int i = 0; i < INK_TEXT_PAGE; i++) if (lv_obj_is_visible(K.cand_btn[i])) {
        snprintf(ids[i], sizeof ids[i], "candidate:%d", K.first + i);
        snprintf(candidates[i], sizeof candidates[i], "候选[%d]=%.90s", K.first + i, lv_label_get_text(K.cand_label[i]));
        fields[count++] = (ink_ui_field_spec_t){.id = ids[i], .role = "button", .label = K.cand_icon[i] && lv_obj_is_visible(K.cand_icon[i]) ? "AI 接话" : "候选", .value = candidates[i]};
    }
    static const char *const CAPS[] = {"text.insert", "text.select", "text.submit", "keyboard.key", "keyboard.pick", "keyboard.candidates", "keyboard.edit", "text.voice_start", "text.voice_finish", "text.voice_cancel", "text.undo", "text.redo", "keyboard.page", "keyboard.panel", "keyboard.category", "keyboard.symbol_page", "keyboard.symbol", "keyboard.alternates"};
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
        if (voice_panel_action(CAPS[i])) actions[i].object = (ink_ui_object_ref_t){"keyboard:voice", K.voice_revision};
    }
    ink_ui_page_spec_t page = {.id = ink_ui_action_page(), .title = "键盘", .object = {"keyboard", K.revision},
        .fields = fields, .field_count = count, .actions = actions, .action_count = sizeof actions / sizeof *actions};
    return ink_ui_semantics_summary(&page, buf, cap);
}
static bool overlay_key(ink_key_t k, bool lng) {
    if (!lng && (k == INK_KEY_HOME || k == INK_KEY_BACK)) ink_text_blur(0);
    if (!lng && (k == INK_KEY_NEXT || k == INK_KEY_PREV)) invoke("text.page", k == INK_KEY_NEXT ? "{\"direction\":1}" : "{\"direction\":-1}");
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

static lv_obj_t *new_row(void) {
    lv_obj_t *r = lv_obj_create(K.keys);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(r, ink_ui_px(INK_GAP_XS), 0);
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
static void map_row(const char *letters) {
    for (const char *p = letters; *p; p++) {
        char text[2] = {K.shift && !K.symbols && *p >= 'a' && *p <= 'z' ? (char)(*p - 'a' + 'A') : *p, 0};
        const char *punctuation = ink_text_punctuation(*p);
        map_key(*p == KEY_BACKSPACE ? TR("删除") : punctuation ? punctuation : text, *p == ' ' ? 0 : *p, INK_KB_UNIT);
    }
    K.map[K.map_size++] = "\n";
}
static void map_install(int kind) {
    K.buttons = K.map_size = 0;
    _Static_assert(10 + 11 + 9 + 7 <= KEYBOARD_BUTTON_MAX, "largest keyboard map");
    _Static_assert(KEYBOARD_BUTTON_MAX + 5 <= KEYBOARD_MAP_MAX, "map separators and terminator");
    if (kind == 3 || kind == 4) {
        static const char *const nine[] = {"，", "ABC", "DEF", "GHI", "JKL", "MNO", "PQRS", "TUV", "WXYZ"};
        _Static_assert(sizeof nine / sizeof *nine <= KEYBOARD_BUTTON_MAX, "nine-key map");
        for (int i = 0; i < 9; i++) {
            char text[2] = {(char)('1' + i), 0};
            map_key(kind == 4 ? nine[i] : text, kind == 4 && !i ? ',' : text[0], INK_KB_UNIT);
            if (i % 3 == 2) K.map[K.map_size++] = "\n";
        }
        if (kind == 3) {
            map_row(ink_text_type() == INK_TEXT_PHONE ? "+0\b" : ".0\b");
            if (K.plain || K.numeric) {
                map_key("26", KEY_NUMBER, INK_KB_UNIT);
                map_key(TR("符号"), KEY_SWITCH, INK_KB_UNIT);
                K.map[K.map_size++] = "\n";
            } else map_row(ink_text_type() == INK_TEXT_PHONE ? "*#" : "- ");
            K.map_size--; map_key(enter_label(), KEY_ENTER, INK_KB_UNIT);
        }
    } else {
        map_row("qwertyuiop");
        map_key(" ", 0, INK_KB_HALF);
        map_row("asdfghjkl");
        K.map_size--; map_key(" ", 0, INK_KB_HALF); K.map[K.map_size++] = "\n";
        map_key(TR("大写"), KEY_SHIFT, INK_KB_FUNCTION);
        map_row("zxcvbnm");
        K.map_size--; map_key(TR("删除"), KEY_BACKSPACE, INK_KB_FUNCTION); K.map[K.map_size++] = "\n";
    }
    if (kind != 3) {
        map_key(K.symbols ? TR("字母") : !strcmp(ink_lang_code(), "zh") ? TR("符号") : "123", KEY_SWITCH, INK_KB_FUNCTION);
        if (K.plain) {
            map_key(K.english ? TR("英") : "中", INK_TEXT_KEY_LANGUAGE, INK_KB_UNIT);
            map_key(ink_setting_get("text.nine_key") ? "26" : "9", KEY_LAYOUT, INK_KB_FUNCTION);
            if (kind != 4) map_key(ink_text_punctuation(',') ? ink_text_punctuation(',') : ",", ',', INK_KB_UNIT);
        }
        map_key(TR("空格"), ' ', K.plain ? INK_KB_SPACE + (kind == 4 ? INK_KB_HALF : 0) : INK_KB_SPACE + 2 * INK_KB_UNIT + INK_KB_FUNCTION);
        map_key(kind == 4 ? TR("删除") : ink_text_punctuation('.') ? ink_text_punctuation('.') : ".", kind == 4 ? KEY_BACKSPACE : '.', kind == 4 ? INK_KB_FUNCTION : INK_KB_UNIT);
        map_key(enter_label(), KEY_ENTER, INK_KB_FUNCTION);
    }
    K.map[K.map_size] = "";
    if (kind == 3 || kind == 4) {
        K.map[11] = "";
        lv_keyboard_set_map(K.footer, LV_KEYBOARD_MODE_USER_2, K.map + 12, K.control + 9);
        lv_keyboard_set_mode(K.footer, LV_KEYBOARD_MODE_USER_2);
    }
    lv_keyboard_set_map(K.board, LV_KEYBOARD_MODE_USER_1, K.map, K.control);
    lv_keyboard_set_mode(K.board, LV_KEYBOARD_MODE_USER_1);
    K.drawn_shift = K.shift;
}

static void draw_key_icon(lv_event_t *e) {
    lv_draw_task_t *task = lv_event_get_draw_task(e);
    lv_draw_label_dsc_t *label = lv_draw_task_get_label_dsc(task);
    if (!label || label->base.id1 >= (uint32_t)K.buttons) return;
    unsigned index = label->base.id1 + (lv_event_get_target_obj(e) == K.footer ? 9 : 0);
    char code = K.codes[index];
    bool enter_icon = code == KEY_ENTER && ink_ui_text_width(enter_label(), INK_FONT_LIST, INK_WEIGHT_REGULAR) > INK_KB_FUNCTION_WIDTH;
    if (!enter_icon && code != KEY_BACKSPACE && (code != KEY_SHIFT || K.symbols)) return;
    lv_area_t a; lv_draw_task_get_area(task, &a);
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

/* LVGL invalidates keys on every press even when their pressed style is identical.
 * Suppress that native feedback; edits and layout changes inside on_key still invalidate. */
static bool feedback_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    return code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED ||
           code == LV_EVENT_PRESS_LOST || code == LV_EVENT_LONG_PRESSED_REPEAT;
}
static void before_feedback(lv_event_t *e) { if (feedback_event(e)) lv_display_enable_invalidation(NULL, false); }
static void after_feedback(lv_event_t *e) { if (feedback_event(e)) lv_display_enable_invalidation(NULL, true); }
static lv_obj_t *make_board(int height) {
    lv_obj_t *board = lv_keyboard_create(K.keys);
    lv_obj_remove_event_cb(board, lv_keyboard_def_event_cb);
    lv_obj_remove_style_all(board);
    lv_obj_remove_flag(board, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(board, LV_PCT(100), ink_ui_px(height));
    lv_obj_set_style_pad_row(board, ink_ui_px(INK_KB_GAP), 0);
    lv_obj_set_style_pad_column(board, ink_ui_px(INK_KB_GAP), 0);
    lv_obj_set_style_bg_opa(board, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(board, ink_ui_paper(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(board, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_color(board, ink_ui_ink(), LV_PART_ITEMS);
    lv_obj_set_style_radius(board, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_font(board, font(), LV_PART_ITEMS);
    lv_obj_set_style_text_color(board, ink_ui_ink(), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(board, ink_ui_paper(), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(board, ink_ui_ink(), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_event_cb(board, before_feedback, LV_EVENT_ALL | LV_EVENT_PREPROCESS, NULL);
    lv_obj_add_event_cb(board, after_feedback, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(board, on_key, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(board, pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(board, show_alternates, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_flag(board, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(board, draw_key_icon, LV_EVENT_DRAW_TASK_ADDED, NULL);
    return board;
}

#include "keyboard_picker.h"

static void build(int kind) {
    K.revision++;
    K.layout_revision++;
    bool replacing = K.root != NULL;
    if (replacing) lv_obj_clean(K.root);
    else K.root = lv_obj_create(lv_layer_sys());
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
    lv_obj_set_size(K.keys, LV_PCT(100), ink_ui_px(keyboard_height()));
    lv_obj_set_style_pad_row(K.keys, ink_ui_px(INK_GAP_XS), 0);
    lv_obj_set_flex_flow(K.keys, LV_FLEX_FLOW_COLUMN);
    K.composing = lv_label_create(K.keys);
    lv_obj_set_height(K.composing, ink_ui_px(ink_ui_line_height(INK_FONT_LIST)));
    lv_obj_set_style_text_font(K.composing, font(), 0);
    lv_label_set_text(K.composing, "");
    if (K.password) lv_obj_add_flag(K.composing, LV_OBJ_FLAG_HIDDEN);
    K.edit = new_row();
    static const char *const EDIT[] = {"◀", "▶", TR_NOOP("复制"), TR_NOOP("剪切"), TR_NOOP("粘贴"), TR_NOOP("撤销"), TR_NOOP("重做")};
    for (unsigned i = 0; i < sizeof EDIT / sizeof *EDIT; i++) {
        lv_obj_t *b = ink_ui_pill(K.edit, TR(EDIT[i]), 0, 0, 0, candidates_height(), false, i < 5 ? edit_action : NULL, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, pressed, LV_EVENT_PRESSED, NULL);
        if (i >= 5) ink_shell_bind(b, i == 5 ? "text.undo" : "text.redo", NULL);
        ink_ui_pill_text(b, INK_FONT_NOTE, INK_WEIGHT_MEDIUM);
        lv_obj_set_style_min_width(b, ink_ui_px(ink_ui_text_width(i < 2 ? EDIT[i] : TR(EDIT[i]), INK_FONT_NOTE, INK_WEIGHT_MEDIUM) + 2 * (INK_GAP_XS + INK_STROKE)), 0);
        lv_obj_set_flex_grow(b, i < 2 ? 0 : 1);
    }
    lv_obj_add_flag(K.edit, LV_OBJ_FLAG_HIDDEN);
    K.cands = new_row();
    for (int i = 0; i < INK_TEXT_PAGE; i++) {
        K.cand_btn[i] = lv_button_create(K.cands);
        ink_ui_style_button(K.cand_btn[i]);
        lv_obj_set_style_radius(K.cand_btn[i], 0, 0);
        lv_obj_set_style_pad_all(K.cand_btn[i], ink_ui_px(INK_GAP_XS), 0);
        lv_obj_set_height(K.cand_btn[i], ink_ui_px(candidates_height()));
        lv_obj_set_flex_grow(K.cand_btn[i], 1);
        lv_obj_add_event_cb(K.cand_btn[i], on_cand, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(K.cand_btn[i], pressed, LV_EVENT_PRESSED, NULL);
        K.cand_icon[i] = NULL;
        K.cand_label[i] = lv_label_create(K.cand_btn[i]);
        lv_obj_set_style_text_font(K.cand_label[i], font(), 0);
        lv_obj_set_width(K.cand_label[i], LV_PCT(100));
        lv_label_set_long_mode(K.cand_label[i], LV_LABEL_LONG_WRAP);
        lv_label_set_text(K.cand_label[i], "");
        lv_obj_center(K.cand_label[i]);
        lv_obj_add_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_t *space = K.cand_space = lv_obj_create(K.cands);
    lv_obj_remove_style_all(space);
    lv_obj_set_size(space, 0, 0);
    lv_obj_set_flex_grow(space, 1);
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
    lv_obj_center(ml);
    lv_obj_add_flag(K.more, LV_OBJ_FLAG_HIDDEN);
    K.fold = ink_ui_pill(K.cands, TR("键盘"), 0, 0, INK_PRIMARY, candidates_height(), false, on_fold, NULL);
    lv_obj_set_style_radius(K.fold, 0, 0);
    lv_obj_add_event_cb(K.fold, pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_flag(K.fold, LV_OBJ_FLAG_HIDDEN);
    K.previous = ink_ui_pill(K.cands, "◀", 0, 0, candidates_height(), candidates_height(), false, on_previous, NULL);
    lv_obj_set_style_radius(K.previous, 0, 0);
    lv_obj_add_event_cb(K.previous, pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_flag(K.previous, LV_OBJ_FLAG_HIDDEN);
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
    K.board = make_board(kind == 3 ? 3 * key_height(true) + 2 * INK_KB_GAP : kind == 4 ? 3 * key_height(false) + 2 * INK_KB_GAP :
                         4 * key_height(false) + 3 * INK_KB_GAP);
    K.footer = kind == 3 || kind == 4 ? make_board((kind == 3 ? 2 : 1) * key_height(false) + (kind == 3 ? INK_KB_GAP : 0)) : NULL;
    static const char *const enter_icons[] = {"check", "send", "search", "next"};
    for (int i = 0; i < 3; i++) {
        K.icons[i] = ink_ui_icon(K.board, i == 2 ? enter_icons[ink_text_enter_kind()] : i ? "del" : "shift", INK_ICON_MEDIUM, INK_ICON_LINE, 0, 0, false);
        lv_obj_add_flag(K.icons[i], LV_OBJ_FLAG_HIDDEN);
    }
    map_install(kind);
    K.popup = NULL;
    build_picker();
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
            K.numeric = K.suppress_release = false;
            K.popup = NULL;
            K.expanded = false;
        }
        return;
    }
    ink_text_type_t type = ink_text_type();
    bool secret = type == INK_TEXT_PASSWORD || ink_text_ai_hidden(), present = ink_text_value()[0] != 0;
    if (!secret && K.edit_revision != ink_text_edit_revision()) {
        K.edit_revision = ink_text_edit_revision(); K.revision++; ink_ui_content_changed();
    }
    if (secret && present != K.secret_present) K.revision++;
    K.secret_present = present;
    if (type != INK_TEXT_PLAIN && type != INK_TEXT_URL) K.numeric = false;
    int kind = type == INK_TEXT_NUMBER || type == INK_TEXT_PHONE || K.numeric ? 3 : K.symbols ? 2 :
               type == INK_TEXT_PLAIN && !ink_text_english() && ink_setting_get("text.nine_key") ? 4 : type == INK_TEXT_PASSWORD ? 5 : 1;
    bool plain = type == INK_TEXT_PLAIN, english = ink_text_english();
    bool password = type == INK_TEXT_PASSWORD;
    bool language_changed = K.english != english;
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
    else if (K.drawn_shift != K.shift || language_changed) {
        K.layout_revision++;
        map_install(kind);
        ink_ui_hint_obj(K.keys, INK_REFRESH_FAST);
    }
    ink_voice_state_t vs = ink_text_voice_state();
    sync_voice(vs);
    if (vs != INK_VOICE_IDLE) return;
    lv_label_set_text(K.composing, ink_text_composing()[0] ? kind == 4 ? TR("拼音输入") : ink_text_composing() : ink_text_voice_notice());
    ink_text_cand_t c[INK_TEXT_PAGE + 1];
    int n = type == INK_TEXT_PLAIN ? ink_text_candidates(K.first, c, INK_TEXT_PAGE + 1) : 0;
    if (!n && K.first) {
        K.first = 0;
        n = ink_text_candidates(0, c, INK_TEXT_PAGE + 1);
    }
    for (int i = 0; i < INK_TEXT_PAGE; i++) {
        if (i < n) {
            lv_label_set_text(K.cand_label[i], c[i].word);
            if (c[i].ai && !K.cand_icon[i]) {
                K.cand_icon[i] = ink_ui_icon(K.cand_btn[i], "ai", INK_ICON_LIST, INK_ICON_SOLID, 0, 0, i == 0);
                lv_obj_move_to_index(K.cand_icon[i], 0);
                lv_obj_set_flex_flow(K.cand_btn[i], LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(K.cand_btn[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_pad_column(K.cand_btn[i], ink_ui_px(INK_GAP_XS), 0);
            }
            if (K.cand_icon[i]) {
                if (c[i].ai) lv_obj_remove_flag(K.cand_icon[i], LV_OBJ_FLAG_HIDDEN);
                else lv_obj_add_flag(K.cand_icon[i], LV_OBJ_FLAG_HIDDEN);
            }
            lv_obj_set_style_bg_color(K.cand_btn[i], i == 0 ? ink_ui_ink() : ink_ui_paper(), 0);
            lv_obj_set_style_bg_color(K.cand_btn[i], i == 0 ? ink_ui_ink() : ink_ui_paper(), LV_STATE_PRESSED);
            lv_obj_set_style_text_color(K.cand_label[i], i == 0 ? ink_ui_paper() : ink_ui_ink(), 0);
            lv_obj_set_style_border_width(K.cand_btn[i], 0, 0);
            lv_obj_remove_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
        } else lv_obj_add_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (n > 0) lv_obj_remove_flag(K.more, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(K.more, LV_OBJ_FLAG_HIDDEN);
    if (ink_text_voice_possible()) lv_obj_remove_flag(K.mic, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(K.mic, LV_OBJ_FLAG_HIDDEN);
    if (K.shown != n) { K.revision++; ink_ui_content_changed(); }
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
    else {
        lv_obj_add_flag(K.edit, LV_OBJ_FLAG_HIDDEN);
        if (password) lv_obj_add_flag(K.cands, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(K.cands, LV_OBJ_FLAG_HIDDEN);
    }
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
    lv_obj_set_height(K.cands, K.expanded ? ink_ui_px(keyboard_height() - ink_ui_line_height(INK_FONT_LIST) - INK_KB_GAP) : ink_ui_px(candidates_height()));
    lv_obj_update_layout(K.cands);
    int gap = ink_ui_px(INK_GAP_XS);
    int remaining = lv_obj_get_content_width(K.cands) - ink_ui_px(candidates_height()) - gap;
    if (ink_text_voice_possible()) remaining -= ink_ui_px(INK_KB_FUNCTION_WIDTH) + gap;
    for (int i = 0; i < INK_TEXT_PAGE; i++) {
        lv_obj_set_flex_grow(K.cand_btn[i], !K.expanded && i == 0 ? 1 : 0);
        if (i >= n) continue;
        int width = candidate_width(c[i].word, c[i].ai);
        int height = K.expanded ? (lv_obj_get_height(K.cands) - ink_ui_px(candidates_height()) - 2 * gap) / 2 : ink_ui_px(candidates_height());
        lv_obj_set_height(K.cand_btn[i], height);
        lv_obj_set_style_min_width(K.cand_btn[i], candidate_width(c[i].word, c[i].ai), 0);
        lv_obj_set_width(K.cand_btn[i], candidate_width(c[i].word, c[i].ai));
        if (!K.expanded && width + gap > remaining) {
            lv_obj_add_flag(K.cand_btn[i], LV_OBJ_FLAG_HIDDEN);
            remaining = 0;
        } else remaining -= width + gap;
    }
    if (K.expanded) {
        lv_obj_remove_flag(K.fold, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(K.previous, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(K.cand_space, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(K.more, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        if (K.first) lv_obj_remove_state(K.previous, LV_STATE_DISABLED);
        else lv_obj_add_state(K.previous, LV_STATE_DISABLED);
        if (n > INK_TEXT_PAGE) lv_obj_remove_state(K.more, LV_STATE_DISABLED);
        else lv_obj_add_state(K.more, LV_STATE_DISABLED);
    } else {
        lv_obj_add_flag(K.fold, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(K.previous, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(K.cand_space, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_state(K.more, LV_STATE_DISABLED);
        lv_obj_remove_flag(K.more, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    }
    lv_label_set_text(lv_obj_get_child(K.more, 0), K.expanded ? "▶" : "▾");
    ink_ui_hint_obj(K.cands, INK_REFRESH_FAST);
    ink_ui_hint_obj(K.composing, INK_REFRESH_FAST);
}

int ink_shell_keyboard_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "keyboard.key", .title = "按输入法的键（1 符号、2 大写、8 删除、10 回车，其余是 ASCII）", .input = "{\"type\":\"object\",\"properties\":{\"code\":{\"type\":\"integer\",\"description\":\"1 符号；2 大写；3 中英切换（仅普通输入框，先提交拼音）；4 全键/九键（按成员记住）；8 删除；10 回车；其余 ASCII，标点随中英模式\",\"minimum\":1,\"maximum\":127}},\"required\":[\"code\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = press_key},
        {.id = "keyboard.pick", .title = "选输入法候选（从 0 开始）", .input = "{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"index\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = pick},
        {.id = "keyboard.candidates", .title = "展开候选，或 fold 收回键盘", .input = "{\"type\":\"object\",\"properties\":{\"fold\":{\"type\":\"boolean\"}}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = candidates},
        {.id = "keyboard.edit", .title = "编辑选中内容：0 左、1 右、2 复制、3 剪切、4 粘贴、5 撤销", .input = "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":5}},\"required\":[\"action\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = edit},
        {.id = "keyboard.page", .title = "候选翻页", .input = "{\"type\":\"object\",\"properties\":{\"direction\":{\"type\":\"integer\",\"enum\":[-1,1]}},\"required\":[\"direction\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = page_candidates},
        {.id = "keyboard.panel", .title = "切换键盘面板", .input = "{\"type\":\"object\",\"properties\":{\"panel\":{\"type\":\"string\",\"enum\":[\"letters\",\"symbols\",\"number\"]}},\"required\":[\"panel\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = panel_action},
        {.id = "keyboard.category", .title = "符号分类：0 中文、1 数学、2 英文", .input = "{\"type\":\"object\",\"properties\":{\"category\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":2}},\"required\":[\"category\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = panel_action},
        {.id = "keyboard.symbol_page", .title = "符号翻页", .input = "{\"type\":\"object\",\"properties\":{\"direction\":{\"type\":\"integer\",\"enum\":[-1,1]}},\"required\":[\"direction\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = panel_action},
        {.id = "keyboard.symbol", .title = "选符号或长按备选", .input = "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":1}},\"required\":[\"text\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = insert_symbol},
        {.id = "keyboard.alternates", .title = "展开字母备选符号，code 为 ASCII，0 收起", .input = "{\"type\":\"object\",\"properties\":{\"code\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":122}},\"required\":[\"code\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = alternate_action}
    };
    static const ink_setting_def_t recent[] = {
        {.id = "keyboard.recent0", .title = TR_NOOP("最近符号一"), .type = INK_SET_INT, .max = (1LL << 40) - 1, .per_member = true, .child_may_change = true},
        {.id = "keyboard.recent1", .title = TR_NOOP("最近符号二"), .type = INK_SET_INT, .max = (1LL << 40) - 1, .per_member = true, .child_may_change = true}
    };
    if (ink_settings_define(recent, sizeof recent / sizeof *recent)) return -1;
    if (ink_cap_register(caps, sizeof caps / sizeof *caps) != 0) return -1;
    ink_text_set_presenter(sync);
    return 0;
}
