/* Shared geometry and components; no service state lives here. */
/* ui-scale-runtime: dimension x,y,w,h,px,radius,diameter,icon_px,height Public component arguments; each caller is checked. */
/* ui-scale-runtime: dimension offset_x,offset_y,ink_ui_line_height,text_px,moved,descent_at,value_width Theme scaling and font metrics, not independent pixel choices. */
/* ui-scale-runtime: font px Public font argument; ink_ui_font_px and line-height callers are checked. */
/* ui-scale-runtime: icon icon_px Public circle argument; ink_ui_circle callers are checked. */
/* ui-scale-runtime: radius radius Public box argument; ink_ui_box callers are checked. */
#include <stdio.h>
#include <string.h>

#include "ink/hal_assets.h"
#include "ink/osal.h"
#include "ink/sys/font.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/ui_theme.h"

static const int FONT_PX[] = {INK_FONT_STATUS, INK_FONT_NOTE, INK_FONT_LIST, INK_FONT_BODY,
                            INK_FONT_SUBTITLE, INK_FONT_TITLE, INK_FONT_HEADING, INK_FONT_NUMBER, INK_FONT_CLOCK};
static const int FONT_LINE[] = {INK_LINE_STATUS, INK_LINE_NOTE, INK_LINE_LIST, INK_LINE_BODY,
                              INK_LINE_SUBTITLE, INK_LINE_TITLE, INK_LINE_HEADING, INK_LINE_NUMBER, INK_LINE_CLOCK};
enum { GLYPH_SCALE_COST = 8 }; /* a scaled glyph, in plain glyphs: estimate until measured on the device */
static const int TEXT_PERCENT[] = {90, 100, 110, 120}; /* the 字号 setting */
typedef struct { ink_font_t *primary; int px, thicken; } font_face_t;
static lv_font_t fitted[4][3][9] INK_PSRAM_BSS; /* text size, weight, token */
static font_face_t faces[4][3][9] INK_PSRAM_BSS;
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
    dsc->adv_w = (uint16_t)((g.adv * px + source_px / 2) / source_px);
    dsc->box_w = (uint16_t)((g.w * px + source_px - 1) / source_px + (use_fallback ? face->thicken : 0));
    dsc->box_h = (uint16_t)((g.h * px + source_px - 1) / source_px);
    dsc->ofs_x = (int16_t)(g.x_ofs * px / source_px);
    dsc->ofs_y = (int16_t)(-(g.y_ofs + g.h) * px / source_px);
    dsc->format = (lv_font_glyph_format_t)ink_font_bpp(f);
    dsc->stride = 0; dsc->is_placeholder = false;
    dsc->gid.index = cp | (use_fallback ? 0x80000000u : 0);
    dsc->resolved_font = font;
    return true;
}
static int coverage(const uint8_t *bits, int row_bytes, int bpp, int x, int y) {
    int bit = x * bpp;
    return (bits[y * row_bytes + (bit >> 3)] >> (8 - bpp - (bit & 7))) & ((1 << bpp) - 1);
}
static const void *glyph_bitmap(lv_font_glyph_dsc_t *dsc, lv_draw_buf_t *out) {
    const font_face_t *face = dsc->resolved_font->dsc;
    bool fell_back = dsc->gid.index & 0x80000000u;
    int thicken = fell_back ? face->thicken : 0;
    ink_font_t *f = fell_back ? fallback : face->primary;
    ink_glyph_t g;
    if (!ink_font_glyph(f, dsc->gid.index & 0x7fffffffu, &g) || !g.w || !g.h) return NULL;
    ink_font_touch_bitmap(f, &g);
    int bpp = ink_font_bpp(f), levels = (1 << bpp) - 1, row_bytes = (g.w * bpp + 7) / 8;
    int source_px = ink_font_px(f), px = face->px;
    unsigned stride = lv_draw_buf_width_to_stride(dsc->box_w, LV_COLOR_FORMAT_A8);
    if (px == source_px && !thicken) {
        osal_cost_units("glyph.blit", 1);
        for (int y = 0; y < g.h; y++)
            for (int x = 0; x < g.w; x++)
                out->data[(size_t)y * stride + x] = (uint8_t)(coverage(g.bits, row_bytes, bpp, x, y) * 255 / levels);
        return out;
    }
    /* Every other size comes from the same face. Each new pixel averages the source pixels under it:
     * a window as wide as one new pixel, at least one source pixel. Positions in 1/256 source pixel. */
    osal_cost_units("glyph.blit", GLYPH_SCALE_COST);
    int window = source_px * 256 / px;
    if (window < 256) window = 256;
    for (int y = 0; y < dsc->box_h; y++) {
        int y0 = (2 * y + 1) * source_px * 128 / px - window / 2;
        for (int x = 0; x < dsc->box_w; x++) {
            int x0 = (2 * x + 1) * source_px * 128 / px - window / 2, sum = 0;
            for (int sy = y0 >> 8; sy <= (y0 + window - 1) >> 8; sy++) {
                if (sy < 0 || sy >= g.h) continue;
                int wy = LV_MIN(y0 + window, (sy + 1) * 256) - LV_MAX(y0, sy * 256);
                for (int sx = x0 >> 8; sx <= (x0 + window - 1) >> 8; sx++) {
                    if (sx < 0 || sx >= g.w) continue;
                    int wx = LV_MIN(x0 + window, (sx + 1) * 256) - LV_MAX(x0, sx * 256);
                    sum += coverage(g.bits, row_bytes, bpp, sx, sy) * (wx * wy >> 8);
                }
            }
            out->data[(size_t)y * stride + x] = (uint8_t)LV_MIN(255, sum * 255 / levels / (window * window >> 8));
        }
        /* A bold word may have a character only the regular face holds: thicken it, or the word looks broken. */
        uint8_t *row = out->data + (size_t)y * stride;
        for (int x = dsc->box_w - 1; thicken && x > 0; x--)
            for (int k = 1; k <= thicken && k <= x; k++)
                if (row[x - k] > row[x]) row[x] = row[x - k];
    }
    return out;
}

int ink_ui_width(void) { return lv_display_get_horizontal_resolution(NULL); }
int ink_ui_height(void) { return lv_display_get_vertical_resolution(NULL); }
int ink_ui_px(int px) {
    int w = ink_ui_width(), h = ink_ui_height();
    if (w * INK_CANVAS_H <= h * INK_CANVAS_W) return (px * w + INK_CANVAS_W / 2) / INK_CANVAS_W;
    return (px * h + INK_CANVAS_H / 2) / INK_CANVAS_H;
}
static int offset_x(lv_obj_t *parent) {
    return lv_obj_get_width(parent) == ink_ui_width() ? (ink_ui_width() - ink_ui_px(INK_CANVAS_W)) / 2 : 0;
}
static int offset_y(lv_obj_t *parent) {
    return lv_obj_get_height(parent) == ink_ui_height() ? (ink_ui_height() - ink_ui_px(INK_CANVAS_H)) / 2 : 0;
}
lv_color_t ink_ui_paper(void) { return lv_color_white(); }
lv_color_t ink_ui_ink(void) { return lv_color_black(); }
lv_color_t ink_ui_gray(void) { return lv_color_hex(0xe6e6e6); }
lv_color_t ink_ui_secondary(void) { return lv_color_hex(0x5f5f5f); }

/* 字号 moves all text by one factor; a child reads one step above the middle (docs/product/legibility.md).
 * Digit faces do not move, and what is already large does not grow. */
static int text_level(void) {
    if (ink_member_current()->role == INK_ROLE_CHILD) return 2;
    int level = (int)ink_setting_get("display.font_size");
    return level < 0 ? 0 : level > 3 ? 3 : level;
}
static int token_at(int px) {
    for (unsigned i = 0; i < sizeof FONT_PX / sizeof *FONT_PX; i++) if (FONT_PX[i] == px) return (int)i;
    return 0;
}
static int moved(int value, int px, int level) {
    int percent = TEXT_PERCENT[level];
    if (px > INK_FONT_HEADING || (percent > 100 && px >= INK_FONT_TITLE)) return value;
    return (value * percent + 50) / 100;
}
static int text_px(int px, int level) { return moved(px, px, level); }

int ink_ui_line_height(int px) { return moved(FONT_LINE[token_at(px)], px, text_level()); }

int ink_ui_row_double(void) {
    int need = ink_ui_line_height(INK_FONT_LIST) + ink_ui_line_height(INK_FONT_NOTE) + 2 * INK_GAP_XS;
    return need > INK_ROW_DOUBLE ? need : INK_ROW_DOUBLE;
}

static int descent_at(const ink_font_t *f, int px) { return ink_font_descent(f) * px / ink_font_px(f); }

/* Regular text is the reading font, which holds everything; medium and bold hold the UI's own
 * characters at one size each (tools/fontgen/ui_fonts.py); digits have their exact sizes. */
static ink_font_t *face_of(int px, ink_ui_weight_t weight) {
    static ink_font_t *text[3], *digits[3][2];
    ink_font_t **slot = px > INK_FONT_HEADING ? &digits[weight][px == INK_FONT_CLOCK] : &text[weight];
    if (*slot) return *slot;
    char name[64];
    if (px > INK_FONT_HEADING) snprintf(name, sizeof name, "fonts/outfit-%d-%d.inkf", (int)weight, px);
    else if (weight == INK_WEIGHT_REGULAR) snprintf(name, sizeof name, "fonts/sans-44.inkf");
    else snprintf(name, sizeof name, "fonts/ui-%d-%d.inkf", (int)weight, weight == INK_WEIGHT_MEDIUM ? INK_FONT_LIST : INK_FONT_TITLE);
    *slot = ink_font_open(name);
    return *slot;
}

const lv_font_t *ink_ui_font_px(int px, ink_ui_weight_t weight) {
    int level = text_level(), at = token_at(px);
    if ((unsigned)weight > INK_WEIGHT_BOLD) weight = INK_WEIGHT_REGULAR;
    lv_font_t *font = &fitted[level][weight][at];
    if (font->dsc) return font;
    ink_font_t *face = face_of(px, weight);
    if (!face) return &lv_font_montserrat_28;
    if (!fallback) fallback = ink_font_open("fonts/sans-44.inkf");
    *font = *ink_font_lvgl(face);
    font->base_line = (int16_t)ink_ui_px(descent_at(face, text_px(px, level)));
    font->line_height = (uint16_t)ink_ui_px(moved(FONT_LINE[at], px, level));
    if (px == INK_FONT_CLOCK) font->base_line -= ink_ui_px(INK_GAP);
    faces[level][weight][at] = (font_face_t){face, ink_ui_px(text_px(px, level)), weight == INK_WEIGHT_BOLD ? ink_ui_px(text_px(px, level)) / 24 : 0};
    font->dsc = &faces[level][weight][at];
    font->get_glyph_dsc = glyph_desc;
    font->get_glyph_bitmap = glyph_bitmap;
    return font;
}

int ink_ui_text_height(const char *text, int px, ink_ui_weight_t weight, int w) {
    lv_point_t size;
    lv_text_get_size(&size, text, ink_ui_font_px(px, weight), 0, 0, ink_ui_px(w), LV_TEXT_FLAG_NONE);
    return size.y * INK_CANVAS_W / ink_ui_px(INK_CANVAS_W);
}

int ink_ui_text_width(const char *text, int px, ink_ui_weight_t weight) {
    lv_point_t size;
    lv_text_get_size(&size, text, ink_ui_font_px(px, weight), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x * INK_CANVAS_W / ink_ui_px(INK_CANVAS_W) + 1;
}

static void box_style(lv_obj_t *o, int x, int y, int w, int h, int radius, ink_ui_surface_t surface, bool border) {
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, ink_ui_px(x) + offset_x(lv_obj_get_parent(o)), ink_ui_px(y) + offset_y(lv_obj_get_parent(o)));
    lv_obj_set_size(o, ink_ui_px(w), ink_ui_px(h));
    lv_obj_set_style_radius(o, radius < 0 ? LV_RADIUS_CIRCLE : ink_ui_px(radius), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, surface == INK_SURFACE_BLACK ? ink_ui_ink() : surface == INK_SURFACE_STATIC ? ink_ui_gray() : ink_ui_paper(), 0);
    lv_obj_set_style_text_color(o, surface == INK_SURFACE_BLACK ? ink_ui_paper() : ink_ui_ink(), 0);
    lv_obj_set_style_border_color(o, ink_ui_ink(), 0);
    lv_obj_set_style_border_width(o, border ? ink_ui_px(INK_STROKE) : 0, 0);
    if (radius) lv_obj_set_style_clip_corner(o, true, 0);
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
    lv_obj_set_pos(l, ink_ui_px(x) + offset_x(parent), ink_ui_px(y) + offset_y(parent));
    if (w > 0) lv_obj_set_width(l, ink_ui_px(w));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, text ? text : "");
    return l;
}

lv_obj_t *ink_ui_line(lv_obj_t *parent, const char *text, int px, ink_ui_weight_t weight, int x, int y, int w) {
    lv_obj_t *l = ink_ui_label(parent, text, px, weight, x, y, w);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_height(l, ink_ui_px(ink_ui_line_height(px)));
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
    lv_obj_set_style_pad_hor(b, ink_ui_px(INK_GAP_XS), 0);
    lv_obj_t *l = ink_ui_line(b, text, INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, 0, 0);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    return b;
}

void ink_ui_pill_text(lv_obj_t *pill, int px, ink_ui_weight_t weight) {
    lv_obj_t *l = lv_obj_get_child(pill, 0);
    lv_obj_set_style_text_font(l, ink_ui_font_px(px, weight), 0);
    lv_obj_set_height(l, ink_ui_px(ink_ui_line_height(px)));
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
    ink_ui_line(row, title, INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2, w - INK_PRIMARY - INK_GAP);
    lv_obj_t *track = ink_ui_box(row, w - INK_PRIMARY, (INK_CONTROL - INK_RADIUS_PANEL) / 2,
                                 INK_PRIMARY, INK_RADIUS_PANEL, -1, on ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, true);
    int knob = INK_RADIUS_PANEL - 2 * INK_GAP_XS;
    ink_ui_box(track, on ? INK_PRIMARY - knob - INK_GAP_XS : INK_GAP_XS, INK_GAP_XS, knob, knob, -1,
               on ? INK_SURFACE_WHITE : INK_SURFACE_BLACK, false);
    return row;
}

lv_obj_t *ink_ui_slider(lv_obj_t *parent, const char *title, int value, int x, int y, int w, lv_event_cb_t cb, void *ctx) {
    if (value < 0) value = 0;
    if (value > 10) value = 10;
    lv_obj_t *b = ink_ui_box(parent, x, y, w, INK_SLIDER, -1, INK_SURFACE_WHITE, true);
    lv_obj_t *fill = ink_ui_box(b, 0, 0, w * value / 10, INK_SLIDER, 0, INK_SURFACE_BLACK, false);
    (void)fill;
    const char *icon = !strcmp(title, "亮度") ? INK_UI_ICON(sun) : !strcmp(title, "音量") ? INK_UI_ICON(volume) : !strcmp(title, "暖光") ? INK_UI_ICON(warm) : NULL;
    int title_x = INK_GAP_XL + (icon ? INK_ICON_MEDIUM + INK_GAP_M : 0);
    if (icon) ink_ui_icon(b, icon, INK_ICON_MEDIUM, INK_ICON_LINE, INK_GAP_XL, (INK_SLIDER - INK_ICON_MEDIUM) / 2, false);
    int text_y = (INK_SLIDER - ink_ui_line_height(INK_FONT_LIST)) / 2;
    ink_ui_line(b, title, INK_FONT_LIST, INK_WEIGHT_MEDIUM, title_x, text_y, w - 2 * INK_BLOCK_L);
    lv_obj_t *clip = lv_obj_create(b);
    lv_obj_remove_style_all(clip);
    lv_obj_remove_flag(clip, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(clip, LV_OBJ_FLAG_USER_1); /* cuts the white copy of the title where the bar ends, on purpose */
    lv_obj_set_pos(clip, 0, 0);
    lv_obj_set_size(clip, ink_ui_px(w * value / 10), ink_ui_px(INK_SLIDER));
    if (icon) ink_ui_icon(clip, icon, INK_ICON_MEDIUM, INK_ICON_LINE, INK_GAP_XL, (INK_SLIDER - INK_ICON_MEDIUM) / 2, true);
    lv_obj_t *white = ink_ui_line(clip, title, INK_FONT_LIST, INK_WEIGHT_MEDIUM, title_x, text_y, w - 2 * INK_BLOCK_L);
    lv_obj_set_style_text_color(white, ink_ui_paper(), 0);
    char s[8];
    snprintf(s, sizeof s, "%d", value);
    lv_obj_t *v = ink_ui_line(b, s, INK_FONT_LIST, INK_WEIGHT_MEDIUM, w - INK_BLOCK_L, text_y, INK_BLOCK);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(v, value == 10 ? ink_ui_paper() : ink_ui_ink(), 0);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    ink_ui_capture_touch(b);
    return b;
}

lv_obj_t *ink_ui_segments(lv_obj_t *parent, const char *const *labels, int count, int selected, int x, int y, int w, lv_event_cb_t cb, void *ctx) {
    lv_obj_t *row = ink_ui_box(parent, x, y, w, INK_SEGMENT, -1, INK_SURFACE_WHITE, true);
    for (int i = 0; i < count; i++) {
        lv_obj_t *b = ink_ui_pill(row, labels[i], INK_GAP_XS - INK_STROKE + i * (w - 2 * INK_GAP_XS) / count, INK_GAP_XS - INK_STROKE,
                                  (w - 2 * INK_GAP_XS) / count, INK_SEGMENT - 2 * INK_GAP_XS, i == selected, cb, ctx);
        if (i != selected) lv_obj_set_style_border_width(b, 0, 0);
    }
    return row;
}

/* A row's title gets what it needs and its value the rest; each is cut, never wrapped out of the row. */
static int value_width(const char *title, const char *value, int room) {
    if (!value || !value[0]) return 0;
    int title_w = ink_ui_text_width(title, INK_FONT_LIST, INK_WEIGHT_REGULAR);
    int value_w = ink_ui_text_width(value, INK_FONT_NOTE, INK_WEIGHT_REGULAR);
    if (title_w + value_w <= room) return value_w;
    return room - title_w > room / 4 ? room - title_w : room / 4;
}

lv_obj_t *ink_ui_list_row(lv_obj_t *parent, const char *icon, const char *title, const char *value,
                         int x, int y, int w, bool two_lines, lv_event_cb_t cb, void *ctx) {
    int h = two_lines ? ink_ui_row_double() : INK_CONTROL;
    lv_obj_t *r = ink_ui_box(parent, x, y, w, h, 0, INK_SURFACE_STATIC, false);
    bind(r, cb, ctx);
    ink_ui_icon(r, icon, INK_ICON_MEDIUM, INK_ICON_LINE, INK_GAP + (INK_CIRCLE_SMALL - INK_ICON_MEDIUM) / 2, (h - INK_ICON_MEDIUM) / 2, false);
    int left = INK_CIRCLE_SMALL + INK_GAP_XL, room = w - left - INK_BLOCK - INK_GAP, value_w = value_width(title, value, room);
    ink_ui_line(r, title, INK_FONT_LIST, INK_WEIGHT_REGULAR, left, (h - ink_ui_line_height(INK_FONT_LIST)) / 2, room - value_w);
    if (value_w) {
        lv_obj_t *l = ink_ui_line(r, value, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w - INK_BLOCK - value_w, (h - ink_ui_line_height(INK_FONT_NOTE)) / 2, value_w);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(l, ink_ui_secondary(), 0);
    }
    if (cb) ink_ui_icon(r, "next", INK_ICON_SMALL, INK_ICON_LINE, w - INK_READING_MARGIN, (h - INK_ICON_SMALL) / 2, false);
    return r;
}

lv_obj_t *ink_ui_panel(lv_obj_t *parent, int height, bool modal) {
    int w = INK_CANVAS_W;
    lv_obj_t *p = ink_ui_box(parent, 0, INK_CANVAS_H - height, w, height + INK_RADIUS_PANEL,
                             INK_RADIUS_PANEL, INK_SURFACE_WHITE, modal);
    lv_obj_set_style_clip_corner(p, true, 0);
    return p;
}

lv_obj_t *ink_ui_pagebar(lv_obj_t *parent, int page, int total, int y, lv_event_cb_t cb) {
    int w = 2 * INK_CONTROL + INK_CIRCLE_LARGE + 2 * INK_GAP;
    lv_obj_t *row = ink_ui_box(parent, (INK_CANVAS_W - w) / 2, y, w, INK_CONTROL, 0, INK_SURFACE_WHITE, false);
    ink_ui_circle(row, "back", 0, 0, INK_CONTROL, INK_ICON_SMALL, INK_SURFACE_WHITE, page > 1 ? cb : NULL, (void *)(intptr_t)-1);
    char s[24];
    snprintf(s, sizeof s, "%d / %d", page, total);
    lv_obj_t *l = ink_ui_line(row, s, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_CONTROL + INK_GAP, (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2, INK_CIRCLE_LARGE);
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
