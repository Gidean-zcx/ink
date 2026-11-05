/* A text field for pages (see ink/sys/textfield.h). The session is the text service's; this shows it. */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/sys/ai.h"
#include "ink/sys/font.h"
#include "ink/sys/textfield.h"
#include "ink/ui_theme.h"

typedef struct {
    lv_obj_t *obj, *label, *menu;
    ink_text_cfg_t cfg;
    char hint[48];
    char *text; /* what it holds: the session's while it has one */
    size_t cap;
    int session;
    size_t window; /* the character the label starts with */
    int mark_at;   /* the place of the mark in the label, -1 none */
    void (*entered)(void *ctx, const char *value);
    void *ctx;
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
static void compose_line(field_t *f, const char *v, const char *c, char *line, size_t cap) {
    size_t start, end;
    ink_text_selection(&start, &end);
    size_t all = 0;
    for (const char *p = v; *p; p++) all += (*p & 0xC0) != 0x80;
    f->window = end > WINDOW_CHARS ? end - WINDOW_CHARS : 0;
    const char *from = char_at(v, f->window), *a = char_at(v, start), *b = char_at(v, end);
    if (start != end) {
        f->mark_at = (int)(start - f->window);
        snprintf(line, cap, "%.*s[%.*s]%s", (int)(a - from), from, (int)(b - a), a, b);
    } else if (end < all || c[0]) {
        f->mark_at = (int)(end - f->window);
        snprintf(line, cap, "%.*s%s|%s", (int)(a - from), from, c, a);
    } else snprintf(line, cap, "%s", from);
}

static void show(field_t *f) {
    const char *v = f->session ? ink_text_value() : f->text;
    const char *c = f->session ? ink_text_composing() : "";
    char line[256];
    f->window = 0;
    f->mark_at = -1;
    if (f->cfg.type == INK_TEXT_PASSWORD) {
        size_t n = strlen(v);
        if (n >= sizeof line) n = sizeof line - 1;
        memset(line, '*', n);
        line[n] = 0;
    } else if (!v[0] && !c[0]) snprintf(line, sizeof line, "%s", f->hint);
    else if (f->session) compose_line(f, v, c, line, sizeof line);
    else snprintf(line, sizeof line, "%s%s", v, c);
    lv_label_set_text(f->label, line);
    ink_ui_hint_obj(f->obj, INK_REFRESH_FAST);
}

static void remember(field_t *f) {
    if (f->session) snprintf(f->text, f->cap + 1, "%s", ink_text_value());
}

static void cl_changed(void *ctx) {
    field_t *f = ctx;
    remember(f);
    show(f);
}

static void cl_entered(void *ctx, const char *value) {
    field_t *f = ctx;
    remember(f);
    if (f->entered) f->entered(f->ctx, value);
}

static void cl_ended(void *ctx) {
    field_t *f = ctx;
    f->session = 0;
    show(f);
}

static const ink_text_client_t CLIENT = {cl_changed, cl_entered, cl_ended};

static void focus(field_t *f) {
    if (f->session) return;
    ink_text_cfg_t c = f->cfg;
    c.initial = f->text;
    c.hint = f->hint;
    f->session = ink_text_focus(&c, &CLIENT, f);
    if (f->session < 0) f->session = 0;
    show(f);
}

static void close_menu(field_t *f) {
    if (f->menu) lv_obj_delete(f->menu);
    f->menu = NULL;
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
    if (f->mark_at >= 0 && letter > (uint32_t)f->mark_at) letter--; /* the mark is not a letter of the text */
    return f->window + letter;
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
    focus(f);
    if (ink_clip_get(clip, INK_CLIP_MAX + 1, 0, NULL) > 0) ink_text_commit_utf8(clip);
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
        focus(f);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        close_menu(f);
        if (f->session && f->cfg.type != INK_TEXT_PASSWORD) ink_text_select_word(place_of(f, e));
        f->menu = lv_obj_create(lv_layer_sys());
        lv_obj_remove_style_all(f->menu);
        lv_obj_remove_flag(f->menu, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(f->menu, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN), ink_ui_px(INK_CONTROL));
        lv_obj_set_flex_flow(f->menu, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(f->menu, ink_ui_px(INK_GAP_S), 0);
        lv_obj_align_to(f->menu, f->obj, LV_ALIGN_OUT_BOTTOM_LEFT, 0, ink_dp(4));
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
