/* A text field for pages (see ink/sys/textfield.h). The session is the text service's; this shows it. */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/sys/ai.h"
#include "ink/sys/font.h"
#include "ink/sys/textfield.h"

typedef struct {
    lv_obj_t *obj, *label, *menu;
    ink_text_cfg_t cfg;
    char hint[48];
    char *text; /* what it holds: the session's while it has one */
    size_t cap;
    int session;
    void (*entered)(void *ctx, const char *value);
    void *ctx;
} field_t;

static const lv_font_t *font(void) {
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    ink_font_t *cjk = ink_font_for_pt(10, pi.dpi);
    return cjk ? ink_font_lvgl(cjk) : ink_ui_font(0);
}

static void show(field_t *f) {
    const char *v = f->session ? ink_text_value() : f->text;
    const char *c = f->session ? ink_text_composing() : "";
    char line[256];
    if (f->cfg.type == INK_TEXT_PASSWORD) {
        size_t n = strlen(v);
        if (n >= sizeof line) n = sizeof line - 1;
        memset(line, '*', n);
        line[n] = 0;
    } else if (!v[0] && !c[0]) snprintf(line, sizeof line, "%s", f->hint);
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

static void on_copy(lv_event_t *e) {
    field_t *f = lv_event_get_user_data(e);
    if (f->cfg.type != INK_TEXT_PASSWORD) ink_clip_set(f->text, false, 0 /* the person's own */);
    close_menu(f);
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
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, f);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, font(), 0);
    lv_label_set_text(l, label);
}

static void field_event(lv_event_t *e) {
    field_t *f = lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) { /* a plain click also comes after a long press */
        close_menu(f);
        focus(f);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        close_menu(f);
        f->menu = lv_obj_create(lv_layer_top());
        lv_obj_remove_flag(f->menu, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(f->menu, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(f->menu, LV_FLEX_FLOW_ROW);
        lv_obj_align_to(f->menu, f->obj, LV_ALIGN_OUT_BOTTOM_LEFT, 0, ink_dp(4));
        menu_button(f, "复制", on_copy);
        menu_button(f, "粘贴", on_paste);
        menu_button(f, "问 AI", on_ask);
        ink_ui_hint_obj(f->menu, INK_REFRESH_FAST);
    } else if (code == LV_EVENT_DELETE) {
        close_menu(f);
        if (f->session) ink_text_blur(f->session);
        osal_free(f->text);
        osal_free(f);
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
    lv_obj_remove_flag(f->obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(f->obj, LV_PCT(90), ink_dp(48));
    lv_obj_set_style_border_color(f->obj, lv_color_black(), 0);
    lv_obj_set_style_border_width(f->obj, 2, 0);
    lv_obj_set_style_radius(f->obj, ink_dp(4), 0);
    f->label = lv_label_create(f->obj);
    lv_obj_set_style_text_font(f->label, font(), 0);
    lv_obj_set_width(f->label, LV_PCT(100));
    lv_label_set_long_mode(f->label, LV_LABEL_LONG_CLIP);
    lv_obj_add_event_cb(f->obj, field_event, LV_EVENT_ALL, f);
    lv_obj_set_user_data(f->obj, f);
    show(f);
    if (start) focus(f);
    return f->obj;
}

const char *ink_textfield_text(lv_obj_t *field) {
    field_t *f = lv_obj_get_user_data(field);
    return f ? f->text : "";
}
