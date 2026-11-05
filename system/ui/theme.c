/* Shared geometry and components; no service state lives here. */
#include <stdio.h>
#include <string.h>

#include "ink/hal_assets.h"
#include "ink/osal.h"
#include "ink/sys/font.h"
#include "ink/ui_theme.h"

static const int FONT_PX[] = {INK_FONT_STATUS, INK_FONT_NOTE, INK_FONT_LIST, INK_FONT_BODY,
                            INK_FONT_SUBTITLE, INK_FONT_TITLE, INK_FONT_HEADING, INK_FONT_NUMBER, INK_FONT_CLOCK};
static const int FONT_LINE[] = {INK_LINE_STATUS, INK_LINE_NOTE, INK_LINE_LIST, INK_LINE_BODY,
                              INK_LINE_SUBTITLE, INK_LINE_TITLE, INK_LINE_HEADING, INK_LINE_NUMBER, INK_LINE_CLOCK};
static ink_font_t *fonts[3][9];
static lv_font_t adjusted[3][9];
typedef struct { ink_font_t *primary; int px; } font_face_t;
static font_face_t faces[3][9];
static ink_font_t *fallback;
static lv_obj_t *captures[24];

static bool glyph_desc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t cp, uint32_t next) {
    (void)next;
    if (cp == '\n' || cp == '\r') return false;
    const font_face_t *face = font->dsc;
    bool use_fallback = !ink_font_has(face->primary, cp) && fallback && ink_font_has(fallback, cp);
    ink_font_t *f = use_fallback ? fallback : face->primary;
    ink_glyph_t g;
    if (!ink_font_glyph(f, cp, &g)) return false;
    int source_px = ink_font_px(f), px = face->px;
    dsc->adv_w = (uint16_t)(g.adv * px / source_px);
    dsc->box_w = (uint16_t)(g.w * px / source_px);
    dsc->box_h = (uint16_t)(g.h * px / source_px);
    dsc->ofs_x = (int16_t)(g.x_ofs * px / source_px);
    dsc->ofs_y = (int16_t)(-(g.y_ofs + g.h) * px / source_px);
    dsc->format = (lv_font_glyph_format_t)ink_font_bpp(f);
    dsc->stride = 0; dsc->is_placeholder = false;
    dsc->gid.index = cp | (use_fallback ? 0x80000000u : 0);
    dsc->resolved_font = font;
    return true;
}
static const void *glyph_bitmap(lv_font_glyph_dsc_t *dsc, lv_draw_buf_t *out) {
    const font_face_t *face = dsc->resolved_font->dsc;
    ink_font_t *f = dsc->gid.index & 0x80000000u ? fallback : face->primary;
    ink_glyph_t g;
    if (!ink_font_glyph(f, dsc->gid.index & 0x7fffffffu, &g) || !g.w || !g.h) return NULL;
    ink_font_touch_bitmap(f, &g);
    osal_cost_units("glyph.blit", 1);
    int bpp = ink_font_bpp(f), levels = (1 << bpp) - 1, row_bytes = (g.w * bpp + 7) / 8;
    int source_px = ink_font_px(f), px = face->px;
    unsigned stride = lv_draw_buf_width_to_stride(dsc->box_w, LV_COLOR_FORMAT_A8);
    for (unsigned y = 0; y < dsc->box_h; y++) for (unsigned x = 0; x < dsc->box_w; x++) {
        unsigned sx = x * source_px / px, sy = y * source_px / px, bit = sx * bpp;
        int v = (g.bits[sy * row_bytes + bit / 8] >> (8 - bpp - (bit & 7))) & levels;
        out->data[y * stride + x] = (uint8_t)(v * 255 / levels);
    }
    return out;
}

int ink_ui_width(void) { return lv_display_get_horizontal_resolution(NULL); }
int ink_ui_height(void) { return lv_display_get_vertical_resolution(NULL); }
int ink_ui_px(int px) { return (px * ink_ui_width() + INK_CANVAS_W / 2) / INK_CANVAS_W; }
lv_color_t ink_ui_paper(void) { return lv_color_white(); }
lv_color_t ink_ui_ink(void) { return lv_color_black(); }
lv_color_t ink_ui_gray(void) { return lv_color_hex(0xe6e6e6); }
lv_color_t ink_ui_secondary(void) { return lv_color_hex(0x5f5f5f); }

int ink_ui_line_height(int px) {
    for (unsigned i = 0; i < sizeof FONT_PX / sizeof *FONT_PX; i++) if (FONT_PX[i] == px) return FONT_LINE[i];
    return INK_LINE_LIST;
}

const lv_font_t *ink_ui_font_px(int px, ink_ui_weight_t weight) {
    unsigned at = 0;
    for (unsigned i = 0; i < sizeof FONT_PX / sizeof *FONT_PX; i++) if (FONT_PX[i] == px) at = i;
    if ((unsigned)weight > INK_WEIGHT_BOLD) weight = INK_WEIGHT_REGULAR;
    if (!fonts[weight][at]) {
        char name[64];
        bool digits = px == INK_FONT_NUMBER || px == INK_FONT_CLOCK;
        snprintf(name, sizeof name, "fonts/%s-%d-%d.inkf", digits ? "outfit" : "ui", (int)weight, px);
        fonts[weight][at] = ink_font_open(name);
        if (!fonts[weight][at]) return &lv_font_montserrat_28;
        adjusted[weight][at] = *ink_font_lvgl(fonts[weight][at]);
        adjusted[weight][at].base_line += (ink_ui_line_height(px) - adjusted[weight][at].line_height) / 2;
        adjusted[weight][at].line_height = (uint16_t)ink_ui_line_height(px);
        if (!fallback) fallback = ink_font_open("fonts/sans-32.inkf");
        faces[weight][at] = (font_face_t){fonts[weight][at], px};
        adjusted[weight][at].dsc = &faces[weight][at];
        adjusted[weight][at].get_glyph_dsc = glyph_desc;
        adjusted[weight][at].get_glyph_bitmap = glyph_bitmap;
        if (px == INK_FONT_CLOCK) adjusted[weight][at].base_line -= INK_GAP;
    }
    return &adjusted[weight][at];
}

static void box_style(lv_obj_t *o, int x, int y, int w, int h, int radius, ink_ui_surface_t surface, bool border) {
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, ink_ui_px(x), ink_ui_px(y));
    lv_obj_set_size(o, ink_ui_px(w), ink_ui_px(h));
    lv_obj_set_style_radius(o, radius < 0 ? LV_RADIUS_CIRCLE : ink_ui_px(radius), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, surface == INK_SURFACE_BLACK ? ink_ui_ink() : surface == INK_SURFACE_STATIC ? ink_ui_gray() : ink_ui_paper(), 0);
    lv_obj_set_style_text_color(o, surface == INK_SURFACE_BLACK ? ink_ui_paper() : ink_ui_ink(), 0);
    lv_obj_set_style_border_color(o, ink_ui_ink(), 0);
    lv_obj_set_style_border_width(o, border ? ink_ui_px(INK_STROKE) : 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}
lv_obj_t *ink_ui_box(lv_obj_t *parent, int x, int y, int w, int h, int radius, ink_ui_surface_t surface, bool border) {
    lv_obj_t *o = lv_obj_create(parent);
    box_style(o, x, y, w, h, radius, surface, border);
    return o;
}

lv_obj_t *ink_ui_label(lv_obj_t *parent, const char *text, int px, ink_ui_weight_t weight, int x, int y, int w) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_remove_style_all(l);
    lv_obj_set_style_text_font(l, ink_ui_font_px(px, weight), 0);
    lv_obj_set_style_text_line_space(l, 0, 0);
    lv_obj_set_style_text_color(l, lv_obj_get_style_text_color(parent, LV_PART_MAIN), 0);
    lv_obj_set_pos(l, ink_ui_px(x), ink_ui_px(y));
    if (w > 0) lv_obj_set_width(l, ink_ui_px(w));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, text ? text : "");
    return l;
}

static void bind(lv_obj_t *o, lv_event_cb_t cb, void *ctx) {
    if (!cb) return;
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o, cb, LV_EVENT_CLICKED, ctx);
}

lv_obj_t *ink_ui_card(lv_obj_t *parent, int x, int y, int w, int h, ink_ui_surface_t surface) {
    return ink_ui_box(parent, x, y, w, h, INK_RADIUS_CARD, surface, surface == INK_SURFACE_WHITE);
}

lv_obj_t *ink_ui_pill(lv_obj_t *parent, const char *text, int x, int y, int w, int h, bool primary, lv_event_cb_t cb, void *ctx) {
    lv_obj_t *b = lv_button_create(parent);
    box_style(b, x, y, w, h, -1, primary ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, !primary);
    bind(b, cb, ctx);
    lv_obj_set_style_pad_hor(b, ink_ui_px(INK_GAP), 0);
    lv_obj_t *l = ink_ui_label(b, text, INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, 0, 0);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    return b;
}

lv_obj_t *ink_ui_circle(lv_obj_t *parent, const char *icon, int x, int y, int diameter, int icon_px,
                        ink_ui_surface_t surface, lv_event_cb_t cb, void *ctx) {
    lv_obj_t *b = ink_ui_box(parent, x, y, diameter, diameter, -1, surface, surface == INK_SURFACE_WHITE);
    bind(b, cb, ctx);
    ink_ui_icon(b, icon, icon_px, surface == INK_SURFACE_BLACK ? INK_ICON_SOLID : INK_ICON_LINE,
                (diameter - icon_px) / 2, (diameter - icon_px) / 2, surface == INK_SURFACE_BLACK);
    return b;
}

lv_obj_t *ink_ui_toggle(lv_obj_t *parent, const char *title, bool on, int x, int y, int w, lv_event_cb_t cb, void *ctx) {
    lv_obj_t *row = ink_ui_box(parent, x, y, w, INK_CONTROL, 0, INK_SURFACE_WHITE, false);
    bind(row, cb, ctx);
    ink_ui_label(row, title, INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, (INK_CONTROL - INK_LINE_LIST) / 2, w - INK_PRIMARY - INK_GAP);
    lv_obj_t *track = ink_ui_box(row, w - INK_PRIMARY, (INK_CONTROL - INK_RADIUS_PANEL) / 2,
                                 INK_PRIMARY, INK_RADIUS_PANEL, -1, on ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, true);
    ink_ui_box(track, on ? INK_PRIMARY - INK_LINE_LIST - INK_GAP_XS : INK_GAP_XS, INK_GAP_XS,
                INK_LINE_LIST, INK_LINE_LIST, -1, on ? INK_SURFACE_WHITE : INK_SURFACE_BLACK, false);
    return row;
}

lv_obj_t *ink_ui_slider(lv_obj_t *parent, const char *title, int value, int x, int y, int w, lv_event_cb_t cb, void *ctx) {
    if (value < 0) value = 0;
    if (value > 10) value = 10;
    lv_obj_t *b = ink_ui_box(parent, x, y, w, INK_SLIDER, -1, INK_SURFACE_WHITE, true);
    lv_obj_t *fill = ink_ui_box(b, 0, 0, w * value / 10, INK_SLIDER, 0, INK_SURFACE_BLACK, false);
    (void)fill;
    lv_obj_t *label = ink_ui_label(b, title, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP_XL, (INK_SLIDER - INK_LINE_LIST) / 2, w - 2 * INK_BLOCK_L);
    lv_obj_t *clip = lv_obj_create(b);
    lv_obj_remove_style_all(clip);
    lv_obj_set_pos(clip, 0, 0);
    lv_obj_set_size(clip, ink_ui_px(w * value / 10), ink_ui_px(INK_SLIDER));
    lv_obj_t *white = ink_ui_label(clip, title, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP_XL, (INK_SLIDER - INK_LINE_LIST) / 2, w - 2 * INK_BLOCK_L);
    lv_obj_set_style_text_color(white, ink_ui_paper(), 0);
    (void)label;
    char s[8];
    snprintf(s, sizeof s, "%d", value);
    lv_obj_t *v = ink_ui_label(b, s, INK_FONT_LIST, INK_WEIGHT_MEDIUM, w - INK_BLOCK, (INK_SLIDER - INK_LINE_LIST) / 2, INK_GAP_XL);
    lv_obj_set_style_text_color(v, value == 10 ? ink_ui_paper() : ink_ui_ink(), 0);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    ink_ui_capture_touch(b);
    return b;
}

lv_obj_t *ink_ui_segments(lv_obj_t *parent, const char *const *labels, int count, int selected, int x, int y, int w, lv_event_cb_t cb, void *ctx) {
    lv_obj_t *row = ink_ui_box(parent, x, y, w, INK_SEGMENT, -1, INK_SURFACE_WHITE, true);
    for (int i = 0; i < count; i++) {
        lv_obj_t *b = ink_ui_pill(row, labels[i], INK_GAP_XS + i * (w - 2 * INK_GAP_XS) / count, INK_GAP_XS,
                                  (w - 2 * INK_GAP_XS) / count, INK_SEGMENT - 2 * INK_GAP_XS, i == selected, cb, ctx);
        if (i != selected) lv_obj_set_style_border_width(b, 0, 0);
    }
    return row;
}

lv_obj_t *ink_ui_list_row(lv_obj_t *parent, const char *icon, const char *title, const char *value,
                         int x, int y, int w, bool two_lines, lv_event_cb_t cb, void *ctx) {
    int h = two_lines ? INK_ROW_DOUBLE : INK_CONTROL;
    lv_obj_t *r = ink_ui_box(parent, x, y, w, h, 0, INK_SURFACE_STATIC, false);
    bind(r, cb, ctx);
    ink_ui_circle(r, icon, INK_GAP, (h - INK_CIRCLE_SMALL) / 2, INK_CIRCLE_SMALL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
    ink_ui_label(r, title, INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_CIRCLE_SMALL + INK_GAP_XL,
                 (h - INK_LINE_LIST) / 2, w - 2 * INK_CIRCLE_LARGE);
    if (value) {
        lv_obj_t *l = ink_ui_label(r, value, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w / 2, (h - INK_LINE_NOTE) / 2, w / 2 - INK_BLOCK);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(l, ink_ui_secondary(), 0);
    }
    if (cb) ink_ui_icon(r, "next", INK_ICON_SMALL, INK_ICON_LINE, w - INK_READING_MARGIN, (h - INK_ICON_SMALL) / 2, false);
    return r;
}

lv_obj_t *ink_ui_panel(lv_obj_t *parent, int height, bool modal) {
    int w = INK_CANVAS_W;
    lv_obj_t *p = ink_ui_box(parent, 0, ink_ui_height() * INK_CANVAS_W / ink_ui_width() - height, w, height + INK_RADIUS_PANEL,
                             INK_RADIUS_PANEL, INK_SURFACE_WHITE, modal);
    lv_obj_set_style_clip_corner(p, true, 0);
    return p;
}

lv_obj_t *ink_ui_pagebar(lv_obj_t *parent, int page, int total, int y, lv_event_cb_t cb) {
    int w = 2 * INK_CONTROL + INK_BLOCK_L + 2 * INK_GAP;
    lv_obj_t *row = ink_ui_box(parent, (INK_CANVAS_W - w) / 2, y, w, INK_CONTROL, 0, INK_SURFACE_WHITE, false);
    ink_ui_circle(row, "back", 0, 0, INK_CONTROL, INK_ICON_SMALL, INK_SURFACE_WHITE, page > 1 ? cb : NULL, (void *)(intptr_t)-1);
    char s[24];
    snprintf(s, sizeof s, "%d / %d", page, total);
    lv_obj_t *l = ink_ui_label(row, s, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_CONTROL + INK_GAP, (INK_CONTROL - INK_LINE_LIST) / 2, INK_BLOCK_L);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    ink_ui_circle(row, "next", w - INK_CONTROL, 0, INK_CONTROL, INK_ICON_SMALL, INK_SURFACE_WHITE, page < total ? cb : NULL, (void *)(intptr_t)1);
    return row;
}

lv_obj_t *ink_ui_notice(lv_obj_t *parent, const char *text, int y) {
    lv_obj_t *l = ink_ui_label(parent, text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_color(l, ink_ui_secondary(), 0);
    return l;
}

void ink_ui_busy_control(lv_obj_t *obj, bool busy) {
    lv_obj_set_style_bg_color(obj, busy ? ink_ui_ink() : ink_ui_paper(), 0);
    lv_obj_set_style_text_color(obj, busy ? ink_ui_paper() : ink_ui_ink(), 0);
    ink_ui_hint_obj(obj, INK_REFRESH_FAST);
}

static void capture_deleted(lv_event_t *e) { ink_ui_capture_forget(lv_event_get_target_obj(e)); }
void ink_ui_capture_touch(lv_obj_t *obj) {
    for (unsigned i = 0; i < sizeof captures / sizeof *captures; i++) if (!captures[i]) {
        captures[i] = obj;
        lv_obj_add_event_cb(obj, capture_deleted, LV_EVENT_DELETE, NULL);
        return;
    }
}
void ink_ui_capture_forget(lv_obj_t *root) {
    for (unsigned i = 0; i < sizeof captures / sizeof *captures; i++) if (captures[i] == root) captures[i] = NULL;
}
bool ink_ui_touch_captured(int x, int y) {
    for (unsigned i = 0; i < sizeof captures / sizeof *captures; i++) if (captures[i] && !lv_obj_has_flag(captures[i], LV_OBJ_FLAG_HIDDEN)) {
        lv_area_t a;
        lv_obj_get_coords(captures[i], &a);
        if (x >= a.x1 && x <= a.x2 && y >= a.y1 && y <= a.y2) return true;
    }
    return false;
}
