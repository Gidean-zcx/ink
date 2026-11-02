/* INKF bitmap fonts + LVGL adapter. */
#include <stdio.h>
#include <string.h>

#include "ink/hal_assets.h"
#include "ink/osal.h"
#include "ink/sys/font.h"

#define INKF_HDR 40

struct ink_font {
    const uint8_t *base;
    size_t size;
    uint8_t bpp;
    uint16_t px;
    int16_t ascent, descent;
    uint16_t line_h;
    uint32_t n;
    const uint8_t *index;
    const uint8_t *bitmaps;
    uint32_t bitmap_size;
    uint32_t replacement; /* code point used for missing glyphs */
    lv_font_t lv;
    bool lv_ready;
};

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static const uint8_t *find(const ink_font_t *f, uint32_t cp) {
    uint32_t lo = 0, hi = f->n;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        const uint8_t *e = f->index + 16u * mid;
        hal_asset_access(e, 16); /* binary search in memory-mapped flash */
        uint32_t c = rd32(e);
        if (c == cp) return e;
        if (c < cp) lo = mid + 1;
        else hi = mid;
    }
    return NULL;
}

ink_font_t *ink_font_open(const char *asset) {
    size_t size = 0;
    const uint8_t *d = hal_asset_map(asset, &size);
    if (!d || size < INKF_HDR || memcmp(d, "INKF", 4) || rd16(d + 4) != 1) return NULL;
    const char *prev = osal_mem_tag("fonts");
    ink_font_t *f = osal_calloc(1, sizeof *f, OSAL_MEM_DEFAULT);
    osal_mem_tag(prev);
    if (!f) return NULL;
    f->base = d;
    f->size = size;
    f->bpp = d[6];
    f->px = rd16(d + 8);
    f->ascent = (int16_t)rd16(d + 10);
    f->descent = (int16_t)rd16(d + 12);
    f->line_h = rd16(d + 14);
    f->n = rd32(d + 16);
    uint32_t index_off = rd32(d + 20), bitmap_off = rd32(d + 24);
    f->bitmap_size = rd32(d + 28);
    if (index_off + 16ull * f->n > size || bitmap_off + (uint64_t)f->bitmap_size > size) {
        osal_free(f);
        return NULL;
    }
    f->index = d + index_off;
    f->bitmaps = d + bitmap_off;
    f->replacement = find(f, 0x25A1) ? 0x25A1 : find(f, 0xFF1F) ? 0xFF1F : '?';
    return f;
}

static void fill(const ink_font_t *f, const uint8_t *e, ink_glyph_t *out) {
    out->cp = rd32(e);
    out->w = e[8];
    out->h = e[9];
    out->x_ofs = (int8_t)e[10];
    out->y_ofs = (int8_t)e[11];
    out->adv = e[12];
    out->bits = f->bitmaps + rd32(e + 4);
}

/* Bitmap bytes of a glyph (2/4 bpp, rows padded to bytes). */
static size_t bitmap_bytes(const ink_font_t *f, const ink_glyph_t *g) { return (size_t)g->h * (((size_t)g->w * f->bpp + 7) / 8); }

bool ink_font_has(const ink_font_t *f, uint32_t cp) { return find(f, cp) != NULL; }

bool ink_font_glyph(const ink_font_t *f, uint32_t cp, ink_glyph_t *out) {
    const uint8_t *e = find(f, cp);
    if (!e) e = find(f, f->replacement);
    if (!e) return false;
    fill(f, e, out);
    return true;
}

void ink_font_touch_bitmap(const ink_font_t *f, const ink_glyph_t *g) {
    if (g->w && g->h) hal_asset_access(g->bits, bitmap_bytes(f, g));
}

int ink_font_px(const ink_font_t *f) { return f->px; }
int ink_font_bpp(const ink_font_t *f) { return f->bpp; }
int ink_font_ascent(const ink_font_t *f) { return f->ascent; }
int ink_font_descent(const ink_font_t *f) { return f->descent; }
int ink_font_line_height(const ink_font_t *f) { return f->line_h; }

/* ------------------------------------------------------------ LVGL adapter */

static bool lv_get_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc, uint32_t letter, uint32_t next) {
    (void)next;
    const ink_font_t *f = font->dsc;
    ink_glyph_t g;
    if (letter == '\n' || letter == '\r') return false;
    if (!ink_font_glyph(f, letter, &g)) return false;
    dsc->adv_w = g.adv;
    dsc->box_w = g.w;
    dsc->box_h = g.h;
    dsc->ofs_x = g.x_ofs;
    dsc->ofs_y = (int16_t)(-(g.y_ofs + g.h)); /* LVGL: bottom of the box relative to the baseline, up = positive */
    dsc->format = (lv_font_glyph_format_t)f->bpp;
    dsc->stride = 0;
    dsc->is_placeholder = 0;
    dsc->gid.index = g.cp;
    dsc->resolved_font = font;
    return true;
}

static const void *lv_get_bitmap(lv_font_glyph_dsc_t *dsc, lv_draw_buf_t *draw_buf) {
    const ink_font_t *f = dsc->resolved_font->dsc;
    ink_glyph_t g;
    if (!ink_font_glyph(f, dsc->gid.index, &g) || !g.w || !g.h) return NULL;
    hal_asset_access(g.bits, bitmap_bytes(f, &g));
    osal_cost_units("glyph.blit", 1);
    uint8_t *out = draw_buf->data;
    uint32_t stride_out = lv_draw_buf_width_to_stride(g.w, LV_COLOR_FORMAT_A8);
    int bpp = f->bpp, levels = (1 << bpp) - 1;
    int row_bytes = (g.w * bpp + 7) / 8;
    for (int y = 0; y < g.h; y++) {
        const uint8_t *row = g.bits + (size_t)y * row_bytes;
        for (int x = 0; x < g.w; x++) {
            int bit = x * bpp;
            int v = (row[bit >> 3] >> (8 - bpp - (bit & 7))) & levels;
            out[(size_t)y * stride_out + x] = (uint8_t)(v * 255 / levels);
        }
    }
    return draw_buf;
}

const lv_font_t *ink_font_lvgl(ink_font_t *f) {
    if (!f) return NULL;
    if (!f->lv_ready) {
        memset(&f->lv, 0, sizeof f->lv);
        f->lv.get_glyph_dsc = lv_get_dsc;
        f->lv.get_glyph_bitmap = lv_get_bitmap;
        f->lv.line_height = f->line_h;
        f->lv.base_line = f->descent;
        f->lv.subpx = LV_FONT_SUBPX_NONE;
        f->lv.underline_position = (int8_t)(-f->descent / 2);
        f->lv.underline_thickness = (int8_t)(f->px / 16 > 0 ? f->px / 16 : 1);
        f->lv.dsc = f;
        f->lv_ready = true;
    }
    return &f->lv;
}

/* ------------------------------------------------------------ size choice */

static const int SIZES[] = {24, 32}; /* generated by CMake (tools/fontgen); flash budget ~3 MiB */
static ink_font_t *opened[sizeof SIZES / sizeof SIZES[0]];

ink_font_t *ink_font_for_pt(double pt, int dpi) {
    double want = pt * dpi / 72.0;
    int best = -1;
    double best_d = 1e9;
    for (unsigned i = 0; i < sizeof SIZES / sizeof SIZES[0]; i++) {
        if (!opened[i]) {
            char name[64];
            snprintf(name, sizeof name, "fonts/sans-%d.inkf", SIZES[i]);
            opened[i] = ink_font_open(name);
        }
        if (!opened[i]) continue;
        double d = want > SIZES[i] ? want - SIZES[i] : SIZES[i] - want;
        if (d < best_d) {
            best_d = d;
            best = (int)i;
        }
    }
    return best >= 0 ? opened[best] : NULL;
}
