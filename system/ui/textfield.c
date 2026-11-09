/* A text field for pages (see ink/sys/textfield.h). The session is the text service's; this shows it. */
/* ui-scale-runtime: dimension home.x,home.y,position.y Saved LVGL coordinates, adjusted once to the theme keyboard safe area. */
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
    lv_obj_t *obj, *label, *menu;
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
} field_t;

#define WINDOW_CHARS 30

static const lv_font_t *font(void) {
    return ink_ui_font_px(INK_FONT_BODY, INK_WEIGHT_REGULAR);
}

static const char *char_at(const char *s, size_t chars) {
    while (*s && chars) {
        s++;
        while ((*s & 0xC0) == 0x80) s++;
        chars--;
    }
    return s;
}

/* The text with what is being composed where the cursor is; the cursor or the selection marked unless it is simply at the end
 * (placeholder look, D-64). A long text starts so that the cursor is in view. */
static void line_piece(field_t *f, char *line, size_t cap, size_t *bytes, const char *s, size_t n, size_t at, size_t after) {
    if (*bytes + n >= cap || f->displayed + 1 >= 256) return;
    f->display_map[f->displayed++] = at;
    memcpy(line + *bytes, s, n);
    *bytes += n; line[*bytes] = 0;
    f->display_map[f->displayed] = after;
}

static void compose_line(field_t *f, const char *v, const char *c, char *line, size_t cap) {
    size_t start, end;
    ink_text_selection(&start, &end);
    f->window = start > WINDOW_CHARS ? start - WINDOW_CHARS : 0;
    lv_obj_update_layout(f->obj);
    const char *cursor = char_at(v, start);
    while (f->semantic && f->window < start) {
        const char *first = char_at(v, f->window);
        snprintf(line, cap, "%.*s%s|", (int)(cursor - first), first, start == end ? c : "");
        lv_point_t size;
        lv_text_get_size(&size, line, lv_obj_get_style_text_font(f->label, 0),
                         lv_obj_get_style_text_letter_space(f->label, 0), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x <= lv_obj_get_content_width(f->label)) break;
        f->window++;
    }
    const char *p = char_at(v, f->window);
    size_t bytes = 0, at = f->window;
    f->displayed = 0; f->display_map[0] = at;
    line[0] = 0;
    for (;;) {
        if (at == start && start != end) line_piece(f, line, cap, &bytes, "[", 1, at, at);
        if (at == end && start != end) line_piece(f, line, cap, &bytes, "]", 1, at, at);
        if (start == end && at == end && (*p || *c || f->semantic)) {
            for (const char *q = c; *q;) {
                const char *next = char_at(q, 1);
                line_piece(f, line, cap, &bytes, q, (size_t)(next - q), at, at); q = next;
            }
            line_piece(f, line, cap, &bytes, "|", 1, at, at);
        }
        if (!*p || at >= f->window + WINDOW_CHARS * 2) break;
        const char *next = char_at(p, 1);
        line_piece(f, line, cap, &bytes, p, (size_t)(next - p), at, at + 1);
        p = next; at++;
    }
}

static void show(field_t *f) {
    const char *v = f->session ? ink_text_value() : f->text;
    const char *c = f->session ? ink_text_composing() : "";
    char line[256];
    f->window = 0;
    f->displayed = 0;
    if (f->cfg.type == INK_TEXT_PASSWORD) {
        size_t n = strlen(v);
        if (n >= sizeof line) n = sizeof line - 1;
        memset(line, '*', n);
        line[n] = 0;
    } else if (!v[0] && !c[0] && (!f->semantic || !f->session)) snprintf(line, sizeof line, "%s", f->hint);
    else if (f->session) compose_line(f, v, c, line, sizeof line);
    else snprintf(line, sizeof line, "%s%s", v, c);
    lv_label_set_text(f->label, line);
    if (f->semantic) {
        if (f->session) lv_obj_add_state(f->obj, LV_STATE_FOCUSED);
        else lv_obj_remove_state(f->obj, LV_STATE_FOCUSED);
    }
    ink_ui_hint_obj(f->obj, INK_REFRESH_FAST);
}

static void remember(field_t *f) {
    if (f->session) snprintf(f->text, f->cap + 1, "%s", ink_text_value());
}

static void cl_changed(void *ctx) {
    field_t *f = ctx;
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
    if (f->semantic) { ink_text_blur(f->session); value = f->text; }
    if (f->entered) f->entered(f->ctx, value);
}

static void cl_ended(void *ctx) {
    field_t *f = ctx;
    if (f->semantic) {
        lv_point_t home = f->home;
        lv_obj_set_pos(f->obj, home.x, home.y);
        ink_ui_content_changed();
    }
    f->session = 0;
    show(f);
}

static const ink_text_client_t CLIENT = {cl_changed, cl_entered, cl_ended};

static bool short_current(field_t *f) {
    return ink_ui_page_current() && f->owner_epoch == ink_member_epoch() &&
           lv_obj_get_screen(f->obj) == lv_screen_active() && !strcmp(f->page->id, ink_ui_page_name());
}

static bool focus(field_t *f) {
    if (f->semantic && !short_current(f)) return false;
    if (f->session) return true;
    if (f->semantic) {
        lv_area_t field, keyboard;
        lv_obj_update_layout(f->obj);
        lv_obj_get_coords(f->obj, &field);
        ink_shell_keyboard_area(&keyboard);
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
    if (f->session && f->cfg.type != INK_TEXT_PASSWORD) return f->display_map[letter < f->displayed ? letter : f->displayed];
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
    char *q = f->cfg.type == INK_TEXT_PASSWORD || f->cfg.ai_hidden ? NULL : f->text;
    close_menu(f);
    if (q && q[0]) ink_ai_ask_text(q);
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
    } else if (code == LV_EVENT_LONG_PRESSED) {
        close_menu(f);
        if (f->session && f->cfg.type != INK_TEXT_PASSWORD) ink_text_select_word(place_of(f, e));
        f->menu = lv_obj_create(lv_layer_sys());
        lv_obj_remove_style_all(f->menu);
        lv_obj_remove_flag(f->menu, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(f->menu, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN), ink_ui_px(INK_CONTROL));
        lv_obj_set_flex_flow(f->menu, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(f->menu, ink_ui_px(INK_GAP_S), 0);
        lv_obj_align_to(f->menu, f->obj, LV_ALIGN_OUT_BOTTOM_LEFT, 0, ink_dp(INK_GAP_XS));
        if (f->session && f->cfg.type != INK_TEXT_PASSWORD) {
            menu_button(f, "◀", on_left);
            menu_button(f, "▶", on_right);
            menu_button(f, "撤销", on_undo);
        }
        if (f->cfg.type != INK_TEXT_PASSWORD) menu_button(f, "复制", on_copy);
        menu_button(f, "粘贴", on_paste);
        if (f->cfg.type != INK_TEXT_PASSWORD && !f->cfg.ai_hidden) menu_button(f, "问 AI", on_ask);
        ink_ui_hint_obj(f->menu, INK_REFRESH_FAST);
    } else if (code == LV_EVENT_DELETE) {
        close_menu(f);
        if (f->session) ink_text_blur(f->session);
        if (f->semantic && f->semantic->value == f->text) f->semantic->value = NULL;
        if (f->cfg.type == INK_TEXT_PASSWORD) memset(f->text, 0, f->cap + 1);
        osal_free(f->text);
        osal_free(f);
    } else if (code == LV_EVENT_SIZE_CHANGED) {
        lv_label_set_long_mode(f->label, lv_obj_get_height(f->obj) > ink_ui_px(INK_PRIMARY) ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_CLIP);
    }
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
    f->cap = cap;
    f->text = text;
    snprintf(f->text, cap + 1, "%s", cfg->initial ? cfg->initial : "");
    snprintf(f->hint, sizeof f->hint, "%s", cfg->hint ? cfg->hint : "");
    f->entered = entered;
    f->ctx = ctx;
    f->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(f->obj);
    lv_obj_remove_flag(f->obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(f->obj, LV_PCT(90), ink_ui_px(INK_CONTROL));
    lv_obj_set_style_bg_color(f->obj, ink_ui_paper(), 0);
    lv_obj_set_style_bg_opa(f->obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(f->obj, ink_ui_px(INK_MARGIN), 0);
    lv_obj_set_style_pad_ver(f->obj, ink_ui_px(INK_GAP_L), 0);
    lv_obj_set_style_border_color(f->obj, lv_color_black(), 0);
    lv_obj_set_style_border_width(f->obj, ink_ui_px(INK_STROKE), 0);
    lv_obj_set_style_radius(f->obj, ink_ui_px(INK_RADIUS_CARD), 0);
    f->label = lv_label_create(f->obj);
    lv_obj_set_style_text_font(f->label, font(), 0);
    if (cfg->type == INK_TEXT_PLAIN && cfg->enter == INK_ENTER_SEARCH) {
        lv_obj_set_style_bg_color(f->obj, ink_ui_gray(), 0);
        lv_obj_set_style_border_width(f->obj, 0, 0);
        lv_obj_set_style_radius(f->obj, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_left(f->obj, ink_ui_px(INK_MARGIN + INK_ICON_LIST + INK_GAP_M), 0);
        lv_obj_set_style_pad_ver(f->obj, ink_ui_px((INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2), 0);
        lv_obj_set_style_text_font(f->label, ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR), 0);
        ink_ui_icon(f->obj, "search", INK_ICON_LIST, INK_ICON_LINE, 0 - INK_ICON_LIST - INK_GAP_M, 0, false);
    }
    lv_obj_set_width(f->label, LV_PCT(100));
    lv_label_set_long_mode(f->label, LV_LABEL_LONG_CLIP);
    lv_obj_add_event_cb(f->obj, field_event, LV_EVENT_ALL, f);
    lv_obj_set_user_data(f->obj, f);
    ink_ui_capture_touch(f->obj);
    show(f);
    if (start) focus(f);
    return f->obj;
}

const char *ink_textfield_text(lv_obj_t *field) {
    field_t *f = lv_obj_get_user_data(field);
    return f ? f->text : "";
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
    lv_obj_set_style_outline_width(field, ink_ui_px(INK_STROKE), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(field, ink_ui_px(INK_GAP_XS), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(field, ink_ui_ink(), LV_STATE_FOCUSED);
    show(f);
    return 0;
}

int ink_textfield_short_focus(lv_obj_t *field) {
    field_t *f = lv_obj_get_user_data(field);
    return f && f->semantic && focus(f) ? 0 : -1;
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
    ink_shell_keyboard_area(&keyboard);
    ink_jw_raw(&w, ",\"input\":{\"field\":"); ink_jw_str(&w, f->semantic->id);
    ink_jw_raw(&w, f->session ? ",\"focused\":true,\"rect\":" : ",\"focused\":false,\"rect\":"); rect_json(&w, &area);
    ink_jw_raw(&w, ",\"keyboard_rect\":"); rect_json(&w, &keyboard); ink_jw_raw(&w, "}}");
    if (w.overflow) { out[0] = 0; return -1; }
    return (int)w.len;
}
