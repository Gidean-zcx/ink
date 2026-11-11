/* A text field for pages (see ink/sys/textfield.h). The session is the text service's; this shows it. */
/* ui-scale-runtime: dimension home.x,home.y,position.y,keyboard.y1,lv_obj_get_content_height,body_height,w,y,h Saved coordinates and complete theme text lines within the keyboard safe area. */
#include "ink/sys/i18n.h"
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/sys/ai.h"
#include "ink/sys/font.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/textfield.h"
#include "ink/ui_theme.h"
#include "ink/ui_shell.h"

typedef struct {
    lv_obj_t *obj, *label, *menu, *area, *title, *error, *clear, *icon;
    ink_textfield_kind_t kind;
    ink_text_cfg_t cfg;
    char hint[48];
    char *text; /* what it holds: the session's while it has one */
    size_t cap;
    int session;
    size_t window; /* the character the label starts with */
    size_t display_map[256];
    size_t displayed;
    void (*entered)(void *ctx, const char *value);
    void *ctx;
    const ink_ui_page_spec_t *page;
    ink_ui_field_spec_t *semantic;
    const ink_ui_action_spec_t *focus_action;
    uint32_t owner_epoch;
    lv_point_t home;
    lv_obj_t *pages_label, *save_label;
    size_t page_end;
    unsigned page_no, pages;
    lv_timer_t *saved_timer;
} field_t;

static const lv_font_t *font(void) {
    return ink_ui_font_px(INK_FONT_FIELD, INK_WEIGHT_REGULAR);
}

static const char *char_at(const char *s, size_t chars) {
    while (*s && chars) {
        s++;
        while ((*s & 0xC0) == 0x80) s++;
        chars--;
    }
    return s;
}

static void long_bounds(field_t *f, const char *v, char *scratch) {
    size_t len = strlen(v), start = 0, cursor = ink_text_cursor();
    f->pages = 0;
    do {
        size_t end = start;
        while (end < len) {
            size_t next = ink_text_cluster_end(v, end);
            memcpy(scratch, v + start, next - start); scratch[next - start] = 0;
            lv_point_t size;
            lv_text_get_size(&size, scratch, font(), 0, 0, lv_obj_get_content_width(f->area) - ink_ui_px(INK_GAP_M), LV_TEXT_FLAG_NONE);
            if (end > start && (size.y > lv_obj_get_content_height(f->area) - ink_ui_px(ink_ui_line_height(INK_FONT_BODY)) ||
                ink_text_char_offset(v + start, next - start) > 240)) break;
            end = next;
        }
        f->pages++;
        size_t first = ink_text_char_offset(v, start), last = ink_text_char_offset(v, end);
        if ((cursor >= first && cursor < last) || (end == len && cursor >= first)) {
            f->window = first; f->page_end = last; f->page_no = f->pages;
        }
        start = end;
    } while (start < len);
    char pages[32]; snprintf(pages, sizeof pages, "%u / %u", f->page_no, f->pages);
    lv_label_set_text(f->pages_label, pages);
}

/* Compose the visible text and retain service offsets; LVGL draws the cursor and selection. */
static void line_piece(field_t *f, char *line, size_t cap, size_t *bytes, const char *s, size_t n, size_t at, size_t after) {
    if (*bytes + n >= cap) return;
    if (f->displayed + 1 < 256) {
        f->display_map[f->displayed++] = at;
        f->display_map[f->displayed] = after;
    }
    memcpy(line + *bytes, s, n);
    *bytes += n; line[*bytes] = 0;
}

static void compose_line(field_t *f, const char *v, const char *c, char *line, size_t cap) {
    size_t start, end;
    ink_text_selection(&start, &end);
    if (!f->cfg.draft_page) f->window = 0;
    const char *p = char_at(v, f->window);
    size_t bytes = 0, at = f->window;
    f->displayed = 0; f->display_map[0] = at;
    line[0] = 0;
    for (;;) {
        if (start == end && at == end && (*p || *c || f->semantic || f->cfg.draft_page)) {
            for (const char *q = c; *q;) {
                const char *next = char_at(q, 1);
                line_piece(f, line, cap, &bytes, q, (size_t)(next - q), at, at); q = next;
            }
        }
        if (!*p || (f->cfg.draft_page && at >= f->page_end)) break;
        const char *next = char_at(p, 1);
        line_piece(f, line, cap, &bytes, p, (size_t)(next - p), at, at + 1);
        p = next; at++;
    }
}

static void show(field_t *f) {
    const char *v = f->session ? ink_text_value() : f->text;
    const char *c = f->session ? ink_text_composing() : "";
    char *line = osal_malloc(f->cap + 64, OSAL_MEM_EXTERNAL);
    size_t line_cap = f->cap + 64;
    if (!line) return;
    if (!f->cfg.draft_page) f->window = 0;
    f->displayed = 0;
    if (f->cfg.type == INK_TEXT_PASSWORD) snprintf(line, line_cap, "%s", v);
    else if (f->cfg.draft_page && f->session) {
        lv_obj_update_layout(f->obj); long_bounds(f, v, line); compose_line(f, v, c, line, line_cap);
    } else if (f->session) compose_line(f, v, c, line, line_cap);
    else if (f->cfg.draft_page) snprintf(line, line_cap, "%.*s", (int)(char_at(v, f->page_end) - char_at(v, f->window)), char_at(v, f->window));
    else snprintf(line, line_cap, "%s%s", v, c);
    lv_textarea_set_text(f->area, line);
    size_t start = 0, end = 0;
    if (f->session) ink_text_selection(&start, &end);
    if (f->session) lv_textarea_set_cursor_pos(f->area, (int32_t)(start - f->window + ink_text_char_offset(c, strlen(c))));
    lv_point_t scroll; lv_obj_get_scroll_end(f->area, &scroll);
    lv_obj_scroll_to(f->area, scroll.x, scroll.y, LV_ANIM_OFF);
    lv_label_set_text_selection_start(f->label, start == end ? LV_LABEL_TEXT_SELECTION_OFF : (uint32_t)(start - f->window));
    lv_label_set_text_selection_end(f->label, start == end ? LV_LABEL_TEXT_SELECTION_OFF : (uint32_t)(end - f->window));
    lv_obj_set_style_opa(f->area, f->session ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_state(f->area, LV_STATE_FOCUSED, f->session != 0);
    lv_obj_set_flag(f->clear, LV_OBJ_FLAG_HIDDEN, !v[0] || f->cfg.type == INK_TEXT_PASSWORD || f->cfg.draft_page);
    if (f->cfg.type == INK_TEXT_PASSWORD) memset(line, 0, line_cap);
    osal_free(line);
    if (f->session) lv_obj_add_state(f->obj, LV_STATE_FOCUSED);
    else lv_obj_remove_state(f->obj, LV_STATE_FOCUSED);
    ink_ui_hint_obj(f->obj, INK_REFRESH_FAST);
}

static void remember(field_t *f) {
    if (f->session) snprintf(f->text, f->cap + 1, "%s", ink_text_value());
}

static void cl_changed(void *ctx) {
    field_t *f = ctx;
    if (f->saved_timer) lv_timer_resume(f->saved_timer);
    remember(f);
    show(f);
    if (f->semantic) {
        ink_ui_content_changed();
        lv_obj_send_event(f->obj, LV_EVENT_VALUE_CHANGED, NULL);
    }
}

static void cl_entered(void *ctx, const char *value) {
    field_t *f = ctx;
    remember(f);
    if (f->cfg.draft_page) { ink_text_blur(f->session); ink_ui_pop(); return; }
    if (f->semantic) { ink_text_blur(f->session); value = f->text; }
    if (f->entered) f->entered(f->ctx, value);
}

static void cl_ended(void *ctx) {
    field_t *f = ctx;
    if (f->saved_timer) lv_timer_pause(f->saved_timer);
    f->session = 0;
    if (!f->cfg.draft_page) {
        lv_point_t home = f->home;
        lv_obj_set_pos(f->obj, home.x, home.y);
        ink_ui_content_changed();
    }
    show(f);
}

static void cl_page(void *ctx, int direction) {
    field_t *f = ctx;
    if (direction > 0 && f->page_no < f->pages) ink_text_set_cursor(f->page_end);
    else if (direction < 0 && f->window) ink_text_set_cursor(f->window - 1);
}
static const ink_text_client_t CLIENT = {cl_changed, cl_entered, cl_ended, cl_page};

static bool short_current(field_t *f) {
    return ink_ui_page_current() && f->owner_epoch == ink_member_epoch() &&
           lv_obj_get_screen(f->obj) == lv_screen_active() && !strcmp(f->page->id, ink_ui_page_name());
}

static bool focus(field_t *f) {
    if (f->owner_epoch != ink_member_epoch()) return false;
    if (f->cfg.draft_page && (!ink_ui_page_current() || f->owner_epoch != ink_member_epoch() || strcmp(f->cfg.draft_page, ink_ui_page_name()))) return false;
    if (f->semantic && !short_current(f)) return false;
    if (f->session) return true;
    if (!f->cfg.draft_page) {
        lv_area_t field, keyboard;
        lv_obj_update_layout(f->obj);
        lv_obj_get_coords(f->obj, &field);
        ink_shell_keyboard_area_for(f->cfg.type, &keyboard);
        int top = keyboard.y1 - ink_ui_px(INK_GAP) - lv_area_get_height(&field);
        lv_area_t parent; lv_obj_get_content_coords(lv_obj_get_parent(f->obj), &parent);
        if (top < parent.y1 || field.x1 < 0 || field.x2 > keyboard.x2) return false;
        f->home = (lv_point_t){lv_obj_get_x(f->obj), lv_obj_get_y(f->obj)};
        lv_point_t position = f->home; position.y += top - field.y1;
        if (field.y1 > top) lv_obj_set_y(f->obj, position.y);
    }
    ink_text_cfg_t c = f->cfg;
    c.initial = f->text;
    c.hint = f->hint;
    f->session = ink_text_focus(&c, &CLIENT, f);
    if (f->session < 0) f->session = 0;
    if (f->cfg.draft_page && f->session) { f->cfg.draft_id = ink_text_draft_id(); remember(f); }
    if (f->semantic) {
        if (!f->session) cl_ended(f);
        else ink_ui_content_changed();
    }
    show(f);
    return f->session != 0;
}

static void close_menu(field_t *f) {
    if (f->menu) lv_obj_delete(f->menu);
    f->menu = NULL;
}

static size_t field_index(field_t *f, uint32_t letter) {
    if (f->session && f->cfg.type != INK_TEXT_PASSWORD && letter <= f->displayed) return f->display_map[letter];
    if (f->session && f->cfg.type != INK_TEXT_PASSWORD) {
        size_t start, end; ink_text_selection(&start, &end);
        size_t n = ink_text_char_offset(ink_text_composing(), strlen(ink_text_composing()));
        if (start == end && letter > start) return letter < start + n ? start : letter - n;
    }
    return f->window + letter;
}

/* What the tap or the long press was on, as a place in the text. */
static size_t place_of(field_t *f, lv_event_t *e) {
    lv_point_t p;
    lv_indev_get_point(lv_event_get_indev(e), &p);
    lv_area_t a;
    lv_obj_get_coords(f->label, &a);
    p.x -= a.x1;
    p.y -= a.y1;
    uint32_t letter = lv_label_get_letter_on(f->label, &p, true);
    return field_index(f, letter);
}

static void on_copy(lv_event_t *e) {
    field_t *f = lv_event_get_user_data(e);
    char *what = osal_malloc(INK_CLIP_MAX + 1, OSAL_MEM_EXTERNAL);
    if (what) {
        if (f->session && ink_text_selected(what, INK_CLIP_MAX + 1) > 0) ink_clip_set(what, false, 0 /* the person's own */);
        else if (f->cfg.type != INK_TEXT_PASSWORD) ink_clip_set(f->text, false, 0);
        memset(what, 0, INK_CLIP_MAX + 1);
        osal_free(what);
    }
    close_menu(f);
}

static void on_left(lv_event_t *e) {
    (void)e;
    ink_text_move(-1, true);
}

static void on_right(lv_event_t *e) {
    (void)e;
    ink_text_move(1, true);
}

static void on_undo(lv_event_t *e) {
    field_t *f = lv_event_get_user_data(e);
    close_menu(f);
    ink_text_undo();
}

static void on_paste(lv_event_t *e) {
    field_t *f = lv_event_get_user_data(e);
    char *clip = osal_malloc(INK_CLIP_MAX + 1, OSAL_MEM_EXTERNAL);
    close_menu(f);
    if (!clip) return;
    if (focus(f) && ink_clip_get(clip, INK_CLIP_MAX + 1, 0, NULL) > 0) ink_text_commit_utf8(clip);
    osal_free(clip);
}

static void on_ask(lv_event_t *e) {
    field_t *f = lv_event_get_user_data(e);
    close_menu(f);
    ink_receipt_t rc; ink_cap_invoke_json("text.ask_ai", "{}", INK_SRC_TOUCH, 0, &rc);
}
static void on_word(lv_event_t *e) {
    close_menu(lv_event_get_user_data(e));
    ink_receipt_t rc; ink_cap_invoke_json("text.add_word", "{}", INK_SRC_TOUCH, 0, &rc);
}

static void menu_button(field_t *f, const char *label, lv_event_cb_t cb) {
    lv_obj_t *b = lv_button_create(f->menu);
    ink_ui_style_button(b);
    lv_obj_set_size(b, ink_ui_px(INK_SEGMENT), ink_ui_px(INK_CONTROL));
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, f);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, ink_ui_font_px(INK_FONT_NOTE, INK_WEIGHT_MEDIUM), 0);
    lv_label_set_text(l, label);
}

static void field_icons(field_t *f, int w, int y, int h) {
    lv_obj_set_pos(f->clear, w - ink_ui_px(INK_FIELD_CLEAR), y + (h - ink_ui_px(INK_FIELD_CLEAR)) / 2);
    lv_obj_set_pos(f->icon, ink_ui_px(INK_GAP_L), y + (h - ink_ui_px(INK_FIELD_SEARCH_ICON)) / 2);
}

static void field_event(lv_event_t *e) {
    field_t *f = lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) { /* a plain click also comes after a long press */
        close_menu(f);
        if (f->session) ink_text_set_cursor(place_of(f, e)); /* the first tap is to start typing */
        if (f->semantic) {
            const ink_ui_object_ref_t *object = f->focus_action->object.id ? &f->focus_action->object : &f->page->object;
            ink_ui_action_target_t target = {ink_ui_content_version(), *object};
            ink_receipt_t rc;
            ink_ui_semantics_action(f->page, f->focus_action->id, &target, INK_SRC_TOUCH, &rc);
        } else focus(f);
    } else if (code == LV_EVENT_LONG_PRESSED && f->cfg.type != INK_TEXT_PASSWORD) {
        close_menu(f);
        if (f->session && f->cfg.type != INK_TEXT_PASSWORD) ink_text_select_word(place_of(f, e));
        f->menu = lv_obj_create(lv_layer_sys());
        lv_obj_remove_style_all(f->menu);
        lv_obj_remove_flag(f->menu, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(f->menu, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN), ink_ui_px(2 * INK_CONTROL + INK_GAP_S));
        lv_obj_set_flex_flow(f->menu, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_row(f->menu, ink_ui_px(INK_GAP_S), 0);
        lv_obj_set_style_pad_column(f->menu, ink_ui_px(INK_GAP_S), 0);
        lv_obj_align_to(f->menu, f->obj, LV_ALIGN_OUT_BOTTOM_LEFT, 0, ink_dp(INK_GAP_XS));
        if (f->session && f->cfg.type != INK_TEXT_PASSWORD) {
            menu_button(f, "◀", on_left);
            menu_button(f, "▶", on_right);
            menu_button(f, TR("撤销"), on_undo);
        }
        if (f->cfg.type != INK_TEXT_PASSWORD) menu_button(f, TR("复制"), on_copy);
        menu_button(f, TR("粘贴"), on_paste);
        if (f->cfg.type != INK_TEXT_PASSWORD && !f->cfg.ai_hidden) menu_button(f, TR("问 AI"), on_ask);
        if (f->cfg.type != INK_TEXT_PASSWORD && !f->cfg.ai_hidden) menu_button(f, TR("加单词"), on_word);
        ink_ui_hint_obj(f->menu, INK_REFRESH_FAST);
    } else if (code == LV_EVENT_DELETE) {
        close_menu(f);
        if (f->saved_timer) lv_timer_delete(f->saved_timer);
        if (f->session) ink_text_blur(f->session);
        if (f->semantic && f->semantic->value == f->text) f->semantic->value = NULL;
        if (f->cfg.type == INK_TEXT_PASSWORD) memset(f->text, 0, f->cap + 1);
        osal_free(f->text);
        osal_free(f);
    } else if (code == LV_EVENT_SIZE_CHANGED) {
        /* The kind decides the height; a page may still narrow a field that shares a row with buttons (the SMS
         * reply sits between 展开 and 发送): it starts at the container's width and keeps a width set later. */
        lv_obj_set_height(f->obj, LV_SIZE_CONTENT);
    } else if (code == LV_EVENT_LAYOUT_CHANGED) {
        field_icons(f, lv_obj_get_width(f->area), lv_obj_get_y(f->area), lv_obj_get_height(f->area));
    }
}

int ink_textfield_height(ink_textfield_kind_t kind) {
    int line = ink_ui_line_height(INK_FONT_FIELD);
    return ink_ui_line_height(INK_FONT_FIELD_LABEL) + INK_FIELD_LABEL_GAP +
        (kind == INK_FIELD_BODY ? INK_FIELD_LINES * line + 2 * (INK_FIELD_PAD + INK_FIELD_STROKE) : INK_FIELD_HEIGHT + line - INK_LINE_BODY);
}

void ink_textfield_error(lv_obj_t *obj, const char *message) {
    field_t *f = lv_obj_get_user_data(obj);
    lv_label_set_text(f->error, message ? message : "");
    lv_obj_set_flag(f->error, LV_OBJ_FLAG_HIDDEN, !message || !message[0]);
}

static int body_height(lv_obj_t *obj, ink_text_type_t type) {
    int line = ink_ui_line_height(INK_FONT_FIELD);
    lv_area_t keyboard; ink_shell_keyboard_area_for(type, &keyboard);
    int lines = (keyboard.y1 - lv_obj_get_y_aligned(obj) - ink_ui_px(ink_ui_line_height(INK_FONT_FIELD_LABEL) + INK_FIELD_LABEL_GAP + INK_CONTROL + INK_GAP_L + 2 * (INK_FIELD_PAD + INK_FIELD_STROKE) + INK_GAP_S)) / ink_ui_px(line);
    return (lines > 0 ? lines : 1) * line + 2 * (INK_FIELD_PAD + INK_FIELD_STROKE);
}

void ink_textfield_style(lv_obj_t *obj, ink_textfield_kind_t kind, const char *label) {
    field_t *f = lv_obj_get_user_data(obj);
    kind = f->cfg.type == INK_TEXT_PASSWORD ? INK_FIELD_PASSWORD : kind;
    f->kind = kind;
    if (label) lv_label_set_text(f->title, label);
    lv_obj_remove_flag(f->title, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_one_line(f->area, f->kind != INK_FIELD_BODY);
    int line = ink_ui_line_height(INK_FONT_FIELD);
    int height = ink_textfield_height(kind) - ink_ui_line_height(INK_FONT_FIELD_LABEL) - INK_FIELD_LABEL_GAP;
    if (f->cfg.draft_page) height = body_height(obj, f->cfg.type);
    lv_obj_set_height(f->area, ink_ui_px(height));
    lv_obj_set_style_pad_ver(f->area, ink_ui_px(f->kind == INK_FIELD_BODY ? INK_FIELD_PAD : (height - line) / 2 - INK_FIELD_STROKE), 0);
    lv_obj_set_style_pad_right(f->area, ink_ui_px(f->cfg.draft_page || f->kind == INK_FIELD_PASSWORD ? INK_FIELD_PAD : INK_FIELD_CLEAR + INK_FIELD_PAD), 0);
    lv_obj_set_style_radius(f->area, f->kind == INK_FIELD_SEARCH ? LV_RADIUS_CIRCLE : ink_ui_px(INK_FIELD_RADIUS), 0);
    lv_obj_set_style_pad_left(f->area, ink_ui_px(f->kind == INK_FIELD_SEARCH ? INK_GAP_L + INK_FIELD_SEARCH_ICON + INK_GAP_M : INK_FIELD_PAD), 0);
    lv_obj_set_flag(f->icon, LV_OBJ_FLAG_HIDDEN, f->kind != INK_FIELD_SEARCH);
}

static void clear_field(lv_event_t *e) {
    field_t *f = lv_event_get_user_data(e);
    ink_receipt_t rc;
    if (!f->session && f->semantic) {
        ink_ui_action_target_t target = {ink_ui_content_version(), f->focus_action->object.id ? f->focus_action->object : f->page->object};
        ink_ui_semantics_action(f->page, f->focus_action->id, &target, INK_SRC_TOUCH, &rc);
    } else if (!f->session) focus(f);
    if (!f->session) return;
    ink_cap_invoke_json("text.select", "{\"start\":0,\"end\":2147483647}", INK_SRC_TOUCH, 0, &rc);
    if (rc.status == INK_RC_DONE) ink_cap_invoke_json("text.insert", "{\"text\":\"\"}", INK_SRC_TOUCH, 0, &rc);
}

lv_obj_t *ink_textfield_create(lv_obj_t *parent, const ink_text_cfg_t *cfg, void (*entered)(void *ctx, const char *value), void *ctx, bool start) {
    field_t *f = osal_calloc(1, sizeof *f, OSAL_MEM_EXTERNAL);
    size_t cap = cfg->max ? cfg->max : 1024;
    char *text = osal_malloc(cap + 1, OSAL_MEM_EXTERNAL);
    if (!f || !text) {
        osal_free(f);
        osal_free(text);
        return NULL;
    }
    f->cfg = *cfg;
    f->owner_epoch = ink_member_epoch();
    f->cap = cap;
    f->text = text;
    snprintf(f->text, cap + 1, "%s", cfg->initial ? cfg->initial : "");
    snprintf(f->hint, sizeof f->hint, "%s", cfg->hint ? cfg->hint : "");
    f->entered = entered;
    f->ctx = ctx;
    f->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(f->obj);
    lv_obj_set_style_bg_color(f->obj, ink_ui_paper(), 0);
    lv_obj_set_style_bg_opa(f->obj, LV_OPA_COVER, 0);
    if (!cfg->draft_page) lv_obj_set_style_pad_bottom(f->obj, ink_ui_px(INK_GAP), LV_STATE_FOCUSED);
    lv_obj_remove_flag(f->obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(f->obj, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(f->obj, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN), 0);
    lv_obj_set_x(f->obj, lv_obj_get_width(parent) == ink_ui_width() ? ink_ui_px(INK_MARGIN) : 0);
    lv_obj_set_flex_flow(f->obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f->obj, ink_ui_px(INK_FIELD_LABEL_GAP), 0);
    f->title = ink_ui_label(f->obj, f->hint[0] ? f->hint : TR("内容"), INK_FONT_FIELD_LABEL, INK_WEIGHT_MEDIUM, 0, 0, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_width(f->title, LV_PCT(100));
    f->area = lv_textarea_create(f->obj);
    lv_obj_remove_style_all(f->area);
    lv_obj_remove_flag(f->area, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(f->area, LV_PCT(100));
    lv_obj_set_style_text_font(f->area, font(), 0);
    lv_obj_set_style_text_color(f->area, ink_ui_ink(), 0);
    lv_obj_set_style_text_color(f->area, ink_ui_secondary(), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(f->area, ink_ui_ink(), LV_PART_SELECTED);
    lv_obj_set_style_text_color(f->area, ink_ui_paper(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(f->area, ink_ui_paper(), 0);
    lv_obj_set_style_bg_opa(f->area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(f->area, ink_ui_ink(), 0);
    lv_obj_set_style_border_width(f->area, ink_ui_px(INK_FIELD_STROKE), 0);
    lv_obj_set_style_pad_right(f->area, ink_ui_px(INK_FIELD_CLEAR + INK_FIELD_PAD), 0);
    lv_obj_set_style_outline_width(f->area, ink_ui_px(INK_FIELD_STROKE), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(f->area, 0 - ink_ui_px(INK_FIELD_FOCUS_GAP), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(f->area, ink_ui_ink(), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(f->area, ink_ui_px(INK_FIELD_STROKE), LV_PART_CURSOR);
    lv_obj_set_style_border_color(f->area, ink_ui_ink(), LV_PART_CURSOR);
    lv_obj_set_style_border_side(f->area, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
    lv_obj_set_style_anim_duration(f->area, 0, LV_PART_CURSOR);
    lv_textarea_set_cursor_click_pos(f->area, false);
    lv_textarea_set_placeholder_text(f->area, f->hint);
    lv_textarea_set_password_show_time(f->area, 0);
    lv_textarea_set_password_bullet(f->area, "*");
    lv_textarea_set_password_mode(f->area, cfg->type == INK_TEXT_PASSWORD);
    f->label = lv_textarea_get_label(f->area);
    f->error = ink_ui_label(f->obj, "", INK_FONT_FIELD_LABEL, INK_WEIGHT_REGULAR, 0, 0, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_width(f->error, LV_PCT(100));
    lv_obj_add_flag(f->error, LV_OBJ_FLAG_HIDDEN);
    f->clear = ink_ui_circle(f->obj, "close", 0, 0, INK_FIELD_CLEAR, INK_FIELD_ICON, INK_SURFACE_WHITE, clear_field, f);
    lv_obj_add_flag(f->clear, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_border_width(f->clear, 0, 0);
    f->icon = ink_ui_icon(f->obj, "search", INK_FIELD_SEARCH_ICON, INK_ICON_LINE, 0, 0, false);
    lv_obj_add_flag(f->icon, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_add_event_cb(f->obj, field_event, LV_EVENT_ALL, f);
    lv_obj_set_user_data(f->obj, f);
    ink_textfield_style(f->obj, cfg->draft_page ? INK_FIELD_BODY : cfg->enter == INK_ENTER_SEARCH ? INK_FIELD_SEARCH : INK_FIELD_SINGLE, NULL);
    if (!cfg->draft_page) lv_obj_add_flag(f->title, LV_OBJ_FLAG_HIDDEN);
    ink_ui_capture_touch(f->obj);
    show(f);
    if (start) focus(f);
    return f->obj;
}

const char *ink_textfield_text(lv_obj_t *field) {
    field_t *f = lv_obj_get_user_data(field);
    return f ? f->text : "";
}

static void saved_tick(lv_timer_t *timer) { ink_textfield_long_refresh(((field_t *)lv_timer_get_user_data(timer))->obj); }
lv_obj_t *ink_textfield_long_create(lv_obj_t *parent, const ink_text_cfg_t *cfg) {
    if (!cfg || !cfg->draft_page || cfg->ai_hidden || cfg->type != INK_TEXT_PLAIN) return NULL;
    lv_obj_t *obj = ink_textfield_create(parent, cfg, NULL, NULL, false);
    if (!obj) return NULL;
    field_t *f = lv_obj_get_user_data(obj); f->owner_epoch = ink_member_epoch();
    lv_area_t keyboard; ink_shell_keyboard_area_for(cfg->type, &keyboard);
    int top = ink_ui_px(INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS);
    lv_obj_set_pos(obj, ink_ui_px(INK_MARGIN), top);
    lv_obj_set_style_pad_bottom(obj, ink_ui_px(INK_CONTROL + INK_GAP_L), 0);
    ink_textfield_style(obj, INK_FIELD_BODY, NULL);
    lv_obj_update_layout(obj);
    f->save_label = ink_ui_label(obj, TR("正在保存"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, 0, INK_PRIMARY + INK_CONTROL);
    lv_obj_align(f->save_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    f->pages_label = ink_ui_label(obj, "1 / 1", INK_FONT_NOTE, INK_WEIGHT_MEDIUM, 0, 0, INK_CIRCLE_SMALL);
    lv_obj_align(f->pages_label, LV_ALIGN_BOTTOM_RIGHT, ink_ui_px(0 - INK_CONTROL - INK_GAP_S), ink_ui_px(0 - INK_GAP_L));
    lv_obj_t *prev = ink_ui_circle(obj, "back", 0, 0, INK_CONTROL, INK_ICON_LIST, INK_SURFACE_WHITE, NULL, NULL);
    lv_obj_align(prev, LV_ALIGN_BOTTOM_RIGHT, ink_ui_px(0 - INK_CONTROL - INK_CIRCLE_SMALL - 2 * INK_GAP_S), 0);
    ink_shell_bind(prev, "text.page", "{\"direction\":-1}");
    lv_obj_t *next = ink_ui_circle(obj, "next", 0, 0, INK_CONTROL, INK_ICON_LIST, INK_SURFACE_WHITE, NULL, NULL);
    lv_obj_align(next, LV_ALIGN_BOTTOM_RIGHT, 0, 0); ink_shell_bind(next, "text.page", "{\"direction\":1}");
    lv_obj_t *footer[] = {f->save_label, f->pages_label, prev, next};
    for (unsigned i = 0; i < sizeof footer / sizeof *footer; i++) {
        lv_obj_add_flag(footer[i], LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
        lv_obj_set_style_translate_y(footer[i], ink_ui_px(INK_CONTROL + INK_GAP_L), 0);
    }
    if (!focus(f)) { lv_obj_delete(obj); return NULL; }
    f->saved_timer = lv_timer_create(saved_tick, 250, f);
    return obj;
}
void ink_textfield_long_refresh(lv_obj_t *field) {
    field_t *f = lv_obj_get_user_data(field);
    if (!f || !f->session || !f->save_label) return;
    const char *status = ink_text_draft_saved() < 0 ? TR("字还在，没能存下") : ink_text_draft_saved() ? TR("已存在这台设备") : TR("正在保存");
    if (strcmp(lv_label_get_text(f->save_label), status)) lv_label_set_text(f->save_label, status);
    if (f->saved_timer && ink_text_draft_saved() != 0) lv_timer_pause(f->saved_timer);
}
int ink_textfield_long_snapshot(lv_obj_t *field, char *out, size_t cap) {
    field_t *f = lv_obj_get_user_data(field);
    if (!f || f->owner_epoch != ink_member_epoch() || !ink_ui_page_current()) { if (cap) out[0] = 0; return -1; }
    ink_jw_t w; ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "{\"page\":"); ink_jw_str(&w, f->cfg.draft_page);
    ink_jw_raw(&w, ",\"draft_id\":"); ink_jw_int(&w, f->cfg.draft_id);
    ink_jw_raw(&w, ",\"page_number\":"); ink_jw_int(&w, f->page_no);
    ink_jw_raw(&w, ",\"pages\":"); ink_jw_int(&w, f->pages);
    ink_jw_raw(&w, ",\"start\":"); ink_jw_int(&w, (int64_t)f->window);
    ink_jw_raw(&w, ",\"end\":"); ink_jw_int(&w, (int64_t)f->page_end);
    const char *start = char_at(f->text, f->window);
    ink_jw_raw(&w, ",\"value\":"); ink_jw_strn(&w, start, (size_t)(char_at(f->text, f->page_end) - start));
    ink_jw_raw(&w, f->session ? ",\"actions\":[\"text.page\",\"text.ask_ai\",\"text.add_word\"]}" : ",\"actions\":[]}");
    if (w.overflow) { if (cap) out[0] = 0; return -1; } return (int)w.len;
}

int ink_textfield_bind_short(lv_obj_t *field, const ink_ui_page_spec_t *page,
                             ink_ui_field_spec_t *spec, const ink_ui_action_spec_t *focus_action) {
    field_t *f = lv_obj_get_user_data(field);
    if (!f || f->session || !page || !spec || !focus_action) return -1;
    bool password = f->cfg.type == INK_TEXT_PASSWORD && spec->secret && f->cfg.ai_hidden;
    if (!password && (spec->secret || f->cfg.ai_hidden ||
        (f->cfg.type != INK_TEXT_PLAIN && f->cfg.type != INK_TEXT_URL))) return -1;
    f->page = page; f->semantic = spec; f->focus_action = focus_action;
    f->owner_epoch = ink_member_epoch();
    spec->value = f->text;
    lv_label_set_text(f->title, spec->label);
    show(f);
    return 0;
}

int ink_textfield_short_focus(lv_obj_t *field) {
    field_t *f = lv_obj_get_user_data(field);
    return f && !f->cfg.draft_page && focus(f) ? 0 : -1;
}

static void rect_json(ink_jw_t *w, const lv_area_t *area) {
    ink_jw_raw(w, "{\"x\":"); ink_jw_int(w, area->x1);
    ink_jw_raw(w, ",\"y\":"); ink_jw_int(w, area->y1);
    ink_jw_raw(w, ",\"width\":"); ink_jw_int(w, lv_area_get_width(area));
    ink_jw_raw(w, ",\"height\":"); ink_jw_int(w, lv_area_get_height(area)); ink_jw_raw(w, "}");
}

int ink_textfield_short_snapshot(lv_obj_t *field, char *out, size_t cap) {
    if (!out || !cap) return -1;
    out[0] = 0;
    field_t *f = lv_obj_get_user_data(field);
    if (!f || !f->semantic || !short_current(f)) return -1;
    ink_jw_t w; ink_jw_init(&w, out, cap);
    int n = ink_ui_semantics_snapshot(f->page, out, cap);
    if (n < 1) return -1;
    w.len = (size_t)n - 1;
    lv_area_t area, keyboard;
    lv_obj_update_layout(field); lv_obj_get_coords(field, &area);
    ink_shell_keyboard_area_for(f->cfg.type, &keyboard);
    ink_jw_raw(&w, ",\"input\":{\"field\":"); ink_jw_str(&w, f->semantic->id);
    ink_jw_raw(&w, f->session ? ",\"focused\":true,\"rect\":" : ",\"focused\":false,\"rect\":"); rect_json(&w, &area);
    ink_jw_raw(&w, ",\"keyboard_rect\":"); rect_json(&w, &keyboard); ink_jw_raw(&w, "}}");
    if (w.overflow) { out[0] = 0; return -1; }
    return (int)w.len;
}
