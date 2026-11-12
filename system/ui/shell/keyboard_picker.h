/* Page structure follows the upstream references in docs/tasks/keyboard-2/SOURCE.md. */
/* ui-scale-runtime: dimension candidates_height,keyboard_height,key_height,height,width Geometry comes from keyboard_theme.h and the text scale. */
static const char *const SYMBOL_NAMES[] = {TR_NOOP("中文"), TR_NOOP("数学"), TR_NOOP("英文")};
static const char *const SYMBOLS[] = {
    "，。？！：；、“”‘’…—（）【】《》［］｛｝「」『』～",
    "≈＝≠≌<>≤≥≡()[]{}-+±×*/÷&∥%‰‱°′″∫∮∯∬∭∰∞∑∧∏∈∵∴⊥∝∨∪•√〒∝∽∈∩∧⊙⌒∥∟∣∂∆∞≌∉∪∨⊕⊿⊥∠∫∬∭",
    "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
};
_Static_assert(sizeof SYMBOLS / sizeof *SYMBOLS == sizeof SYMBOL_NAMES / sizeof *SYMBOL_NAMES, "symbol tabs");
static const char *const RECENT_SETTINGS[] = {"keyboard.recent0", "keyboard.recent1"};
static int symbol_columns(void) {
    int columns = (lv_obj_get_content_width(K.keys) + ink_ui_px(INK_KB_GAP)) / ink_ui_px(candidates_height() + INK_KB_GAP);
    return columns > INK_KBD_COLUMNS ? INK_KBD_COLUMNS : columns;
}
static int symbol_page_size(void) { return 2 * symbol_columns(); }
static const char *category_name(void) { return SYMBOL_NAMES[K.category]; }
static bool symbol_at(int category, int index, char text[8]) {
    if (category < 0 || category >= (int)(sizeof SYMBOLS / sizeof *SYMBOLS) || index < 0) return false;
    const char *kbd_source = SYMBOLS[category];
    size_t from = ink_text_byte_offset(kbd_source, (size_t)index), to = ink_text_byte_offset(kbd_source, (size_t)index + 1);
    if (from == to) return false;
    memcpy(text, kbd_source + from, to - from); text[to - from] = 0;
    return true;
}
static int recent_id(int index) {
    return (int)((ink_setting_get(RECENT_SETTINGS[index / 4]) >> ((index % 4) * 10)) & 1023);
}
static void remember_symbol(const char *text, ink_source_t src) {
    if (ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden()) return;
    int id = 0, saved[INK_KBD_RECENT] = {0}, count = 1;
    char word[8];
    for (int cat = 0; cat < (int)(sizeof SYMBOLS / sizeof *SYMBOLS); cat++)
        for (int i = 0; symbol_at(cat, i, word); i++) if (!strcmp(word, text)) { id = cat * 256 + i + 1; break; }
    if (!id) return;
    saved[0] = id;
    for (int i = 0; i < INK_KBD_RECENT && count < INK_KBD_RECENT; i++) {
        int old = recent_id(i);
        if (old && old != id) saved[count++] = old;
    }
    for (int part = 0; part < 2; part++) {
        int64_t packed = 0;
        for (int i = 0; i < 4; i++) packed |= (int64_t)saved[part * 4 + i] << (i * 10);
        ink_receipt_t rc; ink_setting_set(RECENT_SETTINGS[part], packed, src, &rc);
    }
}
static lv_obj_t *picker_button(lv_obj_t *parent, const char *label, const char *cap, const char *args, int width) {
    lv_obj_t *b = ink_ui_pill(parent, label, 0, 0, width, candidates_height(), false, NULL, NULL);
    lv_obj_set_style_min_width(b, ink_ui_px(candidates_height()), 0);
    lv_obj_set_style_radius(b, 0, 0);
    lv_obj_set_style_bg_color(b, ink_ui_paper(), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(b, ink_ui_ink(), LV_STATE_PRESSED);
    if (cap) ink_shell_bind(b, cap, args);
    return b;
}
static lv_obj_t *picker_row(lv_obj_t *parent) {
    lv_obj_t *r = lv_obj_create(parent); lv_obj_remove_style_all(r);
    lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(r, ink_ui_px(INK_KB_GAP), 0);
    return r;
}
static void symbol_button(lv_obj_t *parent, const char *text, int width) {
    char args[64], escaped[16]; size_t at = 0;
    for (const char *kbd_char = text; *kbd_char; kbd_char++) { if (*kbd_char == '"' || *kbd_char == '\\') escaped[at++] = '\\'; escaped[at++] = *kbd_char; }
    escaped[at] = 0;
    snprintf(args, sizeof args, "{\"text\":\"%s\"}", escaped);
    picker_button(parent, text, "keyboard.symbol", args, width);
}
static void sync_picker(void) {
    lv_obj_clean(K.recent); lv_obj_clean(K.symbol_grid); lv_obj_clean(K.picker_nav);
    picker_button(K.recent, TR("最近"), NULL, NULL, candidates_height());
    for (int i = 0; i < INK_KBD_RECENT; i++) {
        int id = recent_id(i) - 1; char text[8];
        if (!K.password && id >= 0 && symbol_at(id / 256, id % 256, text)) symbol_button(K.recent, text, candidates_height());
    }
    int columns = symbol_columns();
    int width = (lv_obj_get_content_width(K.keys) - (columns - 1) * ink_ui_px(INK_KB_GAP)) / columns;
    char text[8];
    for (int i = 0; i < symbol_page_size(); i++) {
        if (!symbol_at(K.category, K.symbol_page * symbol_page_size() + i, text)) break;
        symbol_button(K.symbol_grid, text, width);
    }
    lv_obj_t *prev = picker_button(K.picker_nav, "◀", "keyboard.symbol_page", "{\"direction\":-1}", 0);
    if (!K.symbol_page) lv_obj_add_state(prev, LV_STATE_DISABLED);
    picker_button(K.picker_nav, "26", "keyboard.panel", "{\"panel\":\"letters\"}", 0);
    picker_button(K.picker_nav, "123", "keyboard.panel", "{\"panel\":\"number\"}", 0);
    lv_obj_t *next = picker_button(K.picker_nav, "▶", "keyboard.symbol_page", "{\"direction\":1}", 0);
    if (!symbol_at(K.category, (K.symbol_page + 1) * symbol_page_size(), text)) lv_obj_add_state(next, LV_STATE_DISABLED);
    for (uint32_t i = 0; i < lv_obj_get_child_count(K.picker_nav); i++) lv_obj_set_flex_grow(lv_obj_get_child(K.picker_nav, (int)i), 1);
    for (uint32_t i = 0; i < lv_obj_get_child_count(K.tabs); i++) {
        lv_obj_t *b = lv_obj_get_child(K.tabs, (int)i);
        lv_obj_set_style_text_decor(lv_obj_get_child(b, 0), (int)i == K.category ? LV_TEXT_DECOR_UNDERLINE : LV_TEXT_DECOR_NONE, 0);
    }
}
static void build_picker(void) {
    K.picker = lv_obj_create(K.keys); lv_obj_remove_style_all(K.picker);
    lv_obj_set_size(K.picker, LV_PCT(100), ink_ui_px(keyboard_height()));
    lv_obj_set_flex_flow(K.picker, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(K.picker, ink_ui_px(INK_KB_GAP), 0);
    K.tabs = picker_row(K.picker);
    for (int i = 0; i < (int)(sizeof SYMBOLS / sizeof *SYMBOLS); i++) {
        char args[40]; snprintf(args, sizeof args, "{\"category\":%d}", i);
        lv_obj_t *b = picker_button(K.tabs, TR(SYMBOL_NAMES[i]), "keyboard.category", args, 0);
        lv_obj_set_flex_grow(b, 1);
    }
    K.recent = picker_row(K.picker);
    lv_obj_set_height(K.recent, ink_ui_px(candidates_height()));
    lv_obj_add_flag(K.recent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(K.recent, LV_DIR_HOR);
    K.symbol_grid = picker_row(K.picker);
    lv_obj_set_flex_flow(K.symbol_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(K.symbol_grid, ink_ui_px(INK_KB_GAP), 0);
    lv_obj_set_height(K.symbol_grid, ink_ui_px(2 * candidates_height() + INK_KB_GAP));
    K.picker_nav = picker_row(K.picker);
    lv_obj_update_layout(K.root); sync_picker();
    lv_obj_add_flag(K.picker, LV_OBJ_FLAG_HIDDEN);
}
static void page_candidates(ink_call_t *call) {
    if (!may_edit(call) || !K.expanded) return;
    int64_t direction = 0; ink_args_get_int(&call->args, "direction", &direction);
    int next = K.first + (direction > 0 ? INK_TEXT_PAGE : -INK_TEXT_PAGE);
    ink_text_cand_t candidate;
    if (next >= 0 && ink_text_candidates(next, &candidate, 1)) K.first = next;
    K.layout_revision++; ink_ui_content_changed(); sync();
}
static void panel_action(ink_call_t *call) {
    if (!may_edit(call)) return;
    if (!strcmp(call->cap->id, "keyboard.panel")) {
        char panel[16]; ink_args_get_str(&call->args, "panel", panel, sizeof panel);
        K.symbols = !strcmp(panel, "symbols"); K.numeric = !strcmp(panel, "number"); K.expanded = false;
    } else if (!strcmp(call->cap->id, "keyboard.category")) {
        int64_t category = 0; ink_args_get_int(&call->args, "category", &category);
        K.category = (int)category; K.symbol_page = 0;
    } else {
        int64_t direction = 0; ink_args_get_int(&call->args, "direction", &direction);
        int page = K.symbol_page + (direction > 0 ? 1 : -1); char text[8];
        if (page >= 0 && symbol_at(K.category, page * symbol_page_size(), text)) K.symbol_page = page;
    }
    K.layout_revision++; ink_ui_content_changed();
    if (K.picker) sync_picker();
    sync();
}
static void insert_symbol(ink_call_t *call) {
    if (!may_edit(call)) return;
    char text[8]; if (ink_args_get_str(&call->args, "text", text, sizeof text) <= 0) return;
    if (K.popup) { lv_obj_delete(K.popup); K.popup = NULL; }
    K.alternate[0] = 0;
    ink_text_commit_utf8(text); remember_symbol(text, call->src);
    if (K.symbols) sync_picker();
}
static void alternate_action(ink_call_t *call) {
    if (!may_edit(call)) return;
    int64_t code = 0; ink_args_get_int(&call->args, "code", &code);
    const char *kbd_letters = "qwertyuiopasdfghjklzxcvbnm", *kbd_symbols = "1234567890@*+-=/#()':\"?!~\\";
    const char *kbd_match = code ? strchr(kbd_letters, (int)code) : NULL;
    if (K.popup) { lv_obj_delete(K.popup); K.popup = NULL; }
    K.alternate[0] = 0;
    if (!kbd_match || (K.built_for != 1 && K.built_for != 5) || K.expanded) return;
    K.alternate[0] = kbd_symbols[kbd_match - kbd_letters]; K.alternate[1] = 0;
    K.popup = picker_row(K.root);
    lv_obj_add_flag(K.popup, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(K.popup, ink_ui_px(2 * candidates_height() + INK_KB_GAP), ink_ui_px(candidates_height()));
    lv_obj_set_style_bg_color(K.popup, ink_ui_paper(), 0); lv_obj_set_style_bg_opa(K.popup, LV_OPA_COVER, 0);
    int height = ink_ui_px(0 - 2 * key_height(false));
    lv_obj_align(K.popup, LV_ALIGN_BOTTOM_LEFT, 0, height);
    symbol_button(K.popup, K.alternate, candidates_height());
    picker_button(K.popup, TR("收起"), "keyboard.alternates", "{\"code\":0}", candidates_height());
    ink_ui_hint_obj(K.popup, INK_REFRESH_FAST);
}
static void show_alternates(lv_event_t *e) {
    uint32_t id = lv_keyboard_get_selected_button(lv_event_get_target_obj(e));
    if (id >= (uint32_t)K.buttons) return;
    char args[40]; snprintf(args, sizeof args, "{\"code\":%d}", K.codes[id]);
    invoke("keyboard.alternates", args);
    K.suppress_release = K.popup != NULL;
}
static void picker_snapshot(char *out, size_t cap) {
    char text[8]; size_t used = 0; out[0] = 0;
    if (K.alternate[0]) { snprintf(out, cap, "%s", K.alternate); return; }
    if (!K.symbols) return;
    for (int i = 0; i < symbol_page_size() && used + sizeof text < cap; i++) {
        if (!symbol_at(K.category, K.symbol_page * symbol_page_size() + i, text)) break;
        int n = snprintf(out + used, cap - used, "%s%s", used ? " " : "", text);
        if (n > 0) used += (size_t)n;
    }
}
