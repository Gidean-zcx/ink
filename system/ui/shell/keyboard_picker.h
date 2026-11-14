/* The symbol panel (fcitx PickerWindow: tabs, pages, a bottom row), the long-press alternates (fcitx PopupKeyboardUi)
 * and candidate paging, by this device's rules: tabs wrap, pages turn by buttons with a page number, nothing scrolls. */
/* ui-scale-runtime: dimension cell,width,avail,g,w,page,button,gap,candidates_height,lv_obj_get_content_width,ink_ui_text_width,INK_KBD_POPUP_COLUMNS,keyboard_height,lv_obj_get_content_height Cells come from the keyboard's scaled candidate height. */
#include "keyboard_symbols.h"
static const struct { const char *const *items; int count; } TABLES[] = { /* storage order: a recent id is (table, index) */
    {SYM_CN, sizeof SYM_CN / sizeof *SYM_CN}, {SYM_EN, sizeof SYM_EN / sizeof *SYM_EN},
    {SYM_MATH, sizeof SYM_MATH / sizeof *SYM_MATH}, {SYM_UNIT, sizeof SYM_UNIT / sizeof *SYM_UNIT},
    {SYM_SHAPE, sizeof SYM_SHAPE / sizeof *SYM_SHAPE}, {SYM_ORDINAL, sizeof SYM_ORDINAL / sizeof *SYM_ORDINAL}};
enum { CATEGORIES = 7, RECENT = 0, TABLE_ROWS = 128 }; /* category c > 0 shows table c - 1 */
_Static_assert(sizeof TABLES / sizeof *TABLES == CATEGORIES - 1 && sizeof TABLES / sizeof *TABLES <= 7, "a recent id has 3 table bits");
_Static_assert(sizeof SYM_SHAPE / sizeof *SYM_SHAPE < TABLE_ROWS && sizeof SYM_MATH / sizeof *SYM_MATH < TABLE_ROWS &&
               sizeof SYM_ORDINAL / sizeof *SYM_ORDINAL < TABLE_ROWS && sizeof SYM_EN / sizeof *SYM_EN < TABLE_ROWS, "7 index bits");
static const char *const CATEGORY_NAMES[CATEGORIES] = {TR_NOOP("最近"), TR_NOOP("中文"), TR_NOOP("英文"), TR_NOOP("数学"), TR_NOOP("单位"), TR_NOOP("图形"), TR_NOOP("序号")};
static const char *const RECENT_SETTINGS[] = {"keyboard.recent0", "keyboard.recent1"};
static const char *category_name(void) { return CATEGORY_NAMES[K.category]; }
static int recent_id(int index) { return (int)((ink_setting_get(RECENT_SETTINGS[index / 4]) >> ((index % 4) * 10)) & 1023); }
static const char *symbol_by_id(int id) {
    int table = (id - 1) / TABLE_ROWS, index = (id - 1) % TABLE_ROWS;
    return id > 0 && table < CATEGORIES - 1 && index < TABLES[table].count ? TABLES[table].items[index] : NULL;
}
static const char *symbol_at(int category, int index) {
    if (index < 0 || category < 0 || category >= CATEGORIES) return NULL;
    if (category != RECENT) return index < TABLES[category - 1].count ? TABLES[category - 1].items[index] : NULL;
    for (int i = 0, seen = 0; i < INK_KBD_RECENT && !K.password; i++) {
        const char *sym = symbol_by_id(recent_id(i));
        if (sym && seen++ == index) return sym;
    }
    return NULL;
}
static int symbol_count(int category) { int n = 0; while (symbol_at(category, n)) n++; return n; }
/* fcitx PickerPagesAdapter:77-80: digits are not remembered; nothing typed into a password is. Per member, newest first. */
static void remember_symbol(const char *text, ink_source_t src) {
    if (K.password || ink_text_type() == INK_TEXT_PASSWORD || ink_text_ai_hidden() || (text[0] >= '0' && text[0] <= '9' && !text[1])) return;
    int id = 0, saved[INK_KBD_RECENT] = {0}, count = 1;
    for (int t = 0; t < CATEGORIES - 1 && !id; t++)
        for (int i = 0; i < TABLES[t].count && !id; i++) if (!strcmp(TABLES[t].items[i], text)) id = t * TABLE_ROWS + i + 1;
    if (!id) return;
    saved[0] = id;
    for (int i = 0; i < INK_KBD_RECENT && count < INK_KBD_RECENT; i++) {
        int old = recent_id(i);
        if (old && old != id) saved[count++] = old;
    }
    for (int part = 0; part < 2; part++) {
        int64_t packed = 0;
        for (int i = 0; i < 4; i++) packed |= (int64_t)saved[part * 4 + i] << (i * 10);
        ink_receipt_t rc;
        if (packed != ink_setting_get(RECENT_SETTINGS[part])) ink_setting_set(RECENT_SETTINGS[part], packed, src, &rc);
    }
}
static bool alternate(int letter, int n, char out[16]) { /* the n-th long-press alternate of a letter (PopupPreset) */
    if (letter < 0 || letter >= 52) return false;
    const char *p = POPUP[letter];
    for (; n > 0 && *p; p++) if (*p == ' ') n--;
    size_t len = strcspn(p, " ");
    if (!len || len >= 16) return false;
    memcpy(out, p, len); out[len] = 0;
    return true;
}
static lv_obj_t *picker_row(lv_obj_t *parent) {
    lv_obj_t *r = new_row(parent);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(r, ink_ui_px(gap()), 0);
    lv_obj_set_style_pad_row(r, ink_ui_px(gap()), 0);
    return r;
}
static void sync_picker(void) {
    int cell = ink_ui_px(candidates_height()), g = ink_ui_px(gap());
    for (uint32_t i = 0; i < lv_obj_get_child_count(K.tabs); i++) { /* the open one bold and underlined, not by colour */
        lv_obj_t *tab = lv_obj_get_child(K.tabs, (int32_t)i), *label = lv_obj_get_child(tab, 0);
        bool open = K.actions[ACT_TAB + (int)i].args[12] - '0' == K.category;
        lv_obj_set_style_text_decor(label, open ? LV_TEXT_DECOR_UNDERLINE : LV_TEXT_DECOR_NONE, 0);
        lv_obj_set_style_text_font(label, ink_ui_font_px(INK_FONT_LIST, open ? INK_WEIGHT_BOLD : INK_WEIGHT_REGULAR), 0);
    }
    lv_obj_clean(K.symbol_grid);
    lv_obj_update_layout(K.picker);
    int width = lv_obj_get_content_width(K.picker), columns = LV_MAX(1, (width + g) / (cell + g));
    int avail = lv_obj_get_content_height(K.picker) - lv_obj_get_height(K.tabs) - lv_obj_get_height(K.picker_nav) - 2 * g;
    K.page_size = columns * LV_MAX(1, (avail + g) / (cell + g));
    int count = symbol_count(K.category), pages = LV_MAX(1, (count + K.page_size - 1) / K.page_size);
    if (K.symbol_page >= pages) K.symbol_page = pages - 1;
    lv_obj_set_height(K.symbol_grid, avail);
    for (int i = 0; i < K.page_size; i++) {
        const char *sym = symbol_at(K.category, K.symbol_page * K.page_size + i);
        if (!sym) break;
        char args[48]; text_args(args, sizeof args, sym);
        kb_button(K.symbol_grid, sym, ACT_GRID + i, "keyboard.symbol", args, (width - (columns - 1) * g) / columns);
    }
    if (!count) { lv_obj_t *l = lv_label_create(K.symbol_grid); lv_obj_set_style_text_font(l, font(), 0); lv_label_set_text(l, TR("还没有用过的符号")); }
    char page[32]; snprintf(page, sizeof page, "%d/%d", K.symbol_page + 1, pages);
    set_text(lv_obj_get_child(K.picker_nav, 2), page);
    lv_obj_set_state(lv_obj_get_child(K.picker_nav, 1), LV_STATE_DISABLED, K.symbol_page == 0);
    lv_obj_set_state(lv_obj_get_child(K.picker_nav, 3), LV_STATE_DISABLED, K.symbol_page == pages - 1);
    ink_ui_hint_obj(K.picker, INK_REFRESH_FAST);
}
static void build_picker(void) {
    int cell = ink_ui_px(candidates_height()), g = ink_ui_px(gap());
    K.picker = lv_obj_create(K.keys); lv_obj_remove_style_all(K.picker);
    lv_obj_remove_flag(K.picker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(K.picker, LV_PCT(100), ink_ui_px(keyboard_height()));
    lv_obj_set_flex_flow(K.picker, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(K.picker, ink_ui_px(gap()), 0);
    lv_obj_update_layout(K.root);
    int width = lv_obj_get_content_width(K.keys), first = 0, tabs = CATEGORIES;
    int fit = LV_MAX(1, (width + g) / (cell + g)), rows = (tabs + fit - 1) / fit, per_row = (tabs + rows - 1) / rows;
    K.tabs = picker_row(K.picker); /* fcitx PickerData:13: recent first; tabs wrap into even rows instead of scrolling */
    for (int c = first; c < CATEGORIES && !K.password; c++) { /* a password: ASCII only, no tabs (Mogo ascii_only) */
        char args[24]; snprintf(args, sizeof args, "{\"category\":%d}", c);
        kb_button(K.tabs, TR(CATEGORY_NAMES[c]), ACT_TAB + c - first, "keyboard.category", args, (width - (per_row - 1) * g) / per_row);
    }
    K.symbol_grid = picker_row(K.picker);
    K.picker_nav = picker_row(K.picker); /* fcitx PickerLayout's row, by this device: letters, ◀ page ▶, digits, delete */
    lv_obj_set_flex_align(K.picker_nav, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    static const char *const NAV[][3] = {{"26", "keyboard.panel", "{\"panel\":\"letters\"}"}, {"◀", "keyboard.symbol_page", "{\"direction\":-1}"},
        {"", NULL, NULL}, {"▶", "keyboard.symbol_page", "{\"direction\":1}"}, {"123", "keyboard.panel", "{\"panel\":\"number\"}"},
        {TR_NOOP("删除"), "keyboard.key", "{\"code\":8}"}};
    static const char PAGE_SAMPLE[] = "8/8";
    int page = ink_ui_px(ink_ui_text_width(PAGE_SAMPLE, INK_FONT_LIST, INK_WEIGHT_REGULAR));
    bool one_row = 5 * cell + 5 * g + page <= width; /* at the largest size: 26 ◀ page ▶, then 123 and delete */
    int button = one_row ? (width - page - 5 * g) / 5 : cell;
    for (int i = 0; i < 6; i++) {
        int w = button;
        if (i == 2) w = one_row ? page : width - 3 * cell - 3 * g;
        else if (!one_row && i > 3) w = (width - g) / 2;
        lv_obj_t *b = i == 2 ? lv_label_create(K.picker_nav) : kb_button(K.picker_nav, i == 5 ? TR(NAV[i][0]) : NAV[i][0], ACT_NAV + i - (i > 2), NAV[i][1], NAV[i][2], w);
        if (i == 2) { lv_obj_set_style_text_font(b, font(), 0); lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_CENTER, 0); lv_obj_set_width(b, one_row ? page : width - 3 * cell - 3 * g); lv_label_set_text(b, ""); }
    }
    sync_picker();
}
static void close_popup(void) {
    if (K.popup) {
        lv_area_t a; lv_obj_get_coords(K.popup, &a);
        ink_ui_hint_area(&a, INK_REFRESH_FAST); /* the keys under it come back, nothing else */
        lv_obj_delete(K.popup);
        K.popup = NULL;
        K.revision++;
    }
    K.popup_letter = -1;
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
        char panel[16] = ""; ink_args_get_str(&call->args, "panel", panel, sizeof panel);
        if (nine_composing() && strcmp(panel, "letters")) { notice(TR("先选词或按重输")); return; }
        show_panel(!strcmp(panel, "symbols"), !strcmp(panel, "number"));
        return;
    }
    if (!K.symbols || !K.picker) { call->rc.status = INK_RC_REJECTED; return; }
    if (!strcmp(call->cap->id, "keyboard.category")) {
        int64_t category = 1; ink_args_get_int(&call->args, "category", &category);
        if (category < 0 || category >= CATEGORIES || (K.password && category != 2)) { call->rc.status = INK_RC_REJECTED; return; }
        K.category = (int)category; K.symbol_page = 0;
    } else {
        int64_t direction = 0; ink_args_get_int(&call->args, "direction", &direction);
        int page = K.symbol_page + (direction > 0 ? 1 : -1);
        if (page >= 0 && symbol_at(K.category, page * K.page_size)) K.symbol_page = page;
    }
    K.revision++; ink_ui_content_changed();
    sync_picker();
}
static void insert_symbol(ink_call_t *call) {
    if (!may_edit(call)) return;
    char text[20] = "";
    if (ink_args_get_str(&call->args, "text", text, sizeof text) <= 0 || ink_text_char_offset(text, strlen(text)) > 4) {
        call->rc.status = INK_RC_REJECTED;
        return;
    }
    if (nine_composing()) { notice(TR("先选词或按重输")); return; } /* Mogo :303-304 */
    K.notice = NULL;
    close_popup();
    ink_text_commit_utf8(text);
    remember_symbol(text, call->src);
    if (K.symbols && K.category == RECENT) sync_picker();
}
/* fcitx PopupKeyboardUi, by this device: the whole list at once in cells a finger can hit, chosen by a tap, kept until
 * a choice or 收起; only its own area is drawn. */
static void alternate_action(ink_call_t *call) {
    if (!may_edit(call)) return;
    int64_t code = 0; ink_args_get_int(&call->args, "code", &code);
    close_popup();
    int letter = code >= 'a' && code <= 'z' ? (int)code - 'a' : code >= 'A' && code <= 'Z' ? 26 + (int)code - 'A' : -1;
    if (letter < 0 || (K.built_for != 1 && K.built_for != 5) || K.expanded) { if (code) call->rc.status = INK_RC_REJECTED; return; }
    int cell = ink_ui_px(candidates_height()), g = ink_ui_px(gap());
    int columns = LV_MIN(INK_KBD_POPUP_COLUMNS, (lv_obj_get_content_width(K.keys) + g) / (cell + g));
    K.popup_letter = letter;
    K.popup = picker_row(K.root);
    lv_obj_add_flag(K.popup, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_HIDDEN); /* drawn once, in place */
    lv_obj_set_width(K.popup, columns * cell + (columns - 1) * g + 2 * ink_ui_px(INK_STROKE + INK_GAP_XS));
    lv_obj_set_style_pad_all(K.popup, ink_ui_px(INK_GAP_XS), 0);
    lv_obj_set_style_bg_color(K.popup, ink_ui_paper(), 0); lv_obj_set_style_bg_opa(K.popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(K.popup, ink_ui_px(INK_STROKE), 0);
    lv_obj_set_style_border_color(K.popup, ink_ui_ink(), 0);
    char text[16], args[48];
    int n = 0;
    for (; alternate(letter, n, text) && n < ACT_MAX - ACT_POPUP - 1; n++) {
        text_args(args, sizeof args, text);
        kb_button(K.popup, text, ACT_POPUP + n, "keyboard.symbol", args, cell);
    }
    kb_button(K.popup, TR("收起"), ACT_POPUP + n, "keyboard.alternates", "{\"code\":0}", cell);
    lv_obj_update_layout(K.popup);
    lv_obj_align_to(K.popup, K.board, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_update_layout(K.popup);
    lv_obj_remove_flag(K.popup, LV_OBJ_FLAG_HIDDEN);
    ink_ui_hint_obj(K.popup, INK_REFRESH_FAST);
    K.revision++;
}
static void show_alternates(lv_event_t *e) {
    uint32_t id = lv_keyboard_get_selected_button(lv_event_get_target_obj(e));
    int letter = lv_event_get_target_obj(e) == K.board ? letter_of((int)id) : -1;
    if (letter < 0) return;
    char args[40]; snprintf(args, sizeof args, "{\"code\":%d}", letter < 26 ? 'a' + letter : 'A' + letter - 26);
    invoke("keyboard.alternates", args);
    K.long_pressed = K.popup != NULL; /* the release that follows does not type the letter too */
}
static void picker_snapshot(char *out, size_t cap) {
    size_t used = 0; out[0] = 0;
    char text[16];
    if (K.popup_letter >= 0) { /* the long-press list; the snapshot names it */
        for (int i = 0; alternate(K.popup_letter, i, text) && used + 20 < cap; i++) used += (size_t)snprintf(out + used, cap - used, "%s%s", i ? " " : "", text);
        return;
    }
    if (!K.symbols || !K.page_size) return;
    for (int i = 0; i < K.page_size && used + 20 < cap; i++) {
        const char *sym = symbol_at(K.category, K.symbol_page * K.page_size + i);
        if (!sym) break;
        used += (size_t)snprintf(out + used, cap - used, "%s%.12s", i ? " " : "", sym);
    }
}
