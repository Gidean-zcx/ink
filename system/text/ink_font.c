/* INKF bitmap fonts (v1 packed, v2 compressed glyph by glyph) + LVGL adapter. */
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
    bool runs;            /* INKF v2: every glyph compressed on its own (tools/fontgen/fontgen.py) */
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
    if (!d || size < INKF_HDR || memcmp(d, "INKF", 4) || rd16(d + 4) < 1 || rd16(d + 4) > 2 || (rd16(d + 4) == 2 && d[6] != 2))
        return NULL;
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
    f->runs = rd16(d + 4) == 2;
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
    uint32_t off = rd32(e + 4), stored = f->runs ? rd16(e + 13) : e[9] * ((e[8] * f->bpp + 7u) / 8);
    out->cp = rd32(e);
    out->w = e[8];
    out->h = e[9];
    out->x_ofs = (int8_t)e[10];
    out->y_ofs = (int8_t)e[11];
    out->adv = e[12];
    out->stored = off <= f->bitmap_size && stored <= f->bitmap_size - off ? (uint16_t)stored : 0; /* damaged: blank */
    out->bits = f->bitmaps + (off <= f->bitmap_size ? off : 0);
}

bool ink_font_has(const ink_font_t *f, uint32_t cp) { return find(f, cp) != NULL; }

bool ink_font_glyph(const ink_font_t *f, uint32_t cp, ink_glyph_t *out) {
    const uint8_t *e = find(f, cp);
    if (!e) e = find(f, f->replacement);
    if (!e) return false;
    fill(f, e, out);
    return true;
}

const uint8_t *ink_font_pixels(const ink_font_t *f, const ink_glyph_t *g) {
    static uint8_t px[255 * 255 + 16] INK_PSRAM_BSS; /* the largest glyph INKF holds, and room to write 16 past it */
    uint8_t *o = px, *stop = px + (size_t)g->w * g->h;
    hal_asset_access(g->bits, g->stored); /* lets simulators charge flash cache misses */
    if (!f->runs) { /* v1: rows packed MSB-first */
        int bpp = f->bpp, levels = (1 << bpp) - 1, row_bytes = (g->w * bpp + 7) / 8;
        if ((size_t)g->h * row_bytes > g->stored) return memset(px, 0, (size_t)(stop - o)); /* damaged: paper */
        for (int y = 0; y < g->h; y++)
            for (int x = 0, bit = 0; x < g->w; x++, bit += bpp)
                *o++ = (g->bits[y * row_bytes + (bit >> 3)] >> (8 - bpp - (bit & 7))) & levels;
        return px;
    }
    /* v2: runs over the whole glyph in nibbles, vvnn; nn = 0: 4 + the next nibbles while they are 15. Past the end
     * (damaged): paper. */
    const uint8_t *at = g->bits, *end = g->bits + g->stored;
    unsigned half = 0, k;
#define NIBBLE() (at >= end ? 0u : (half ^= 1) ? (unsigned)*at >> 4 : *at++ & 15u)
    while (o < stop) {
        unsigned code = NIBBLE(), value = code >> 2;
        uint32_t n = code & 3;
        if (!n) {
            n = 4;
            do n += k = NIBBLE(); while (k == 15);
        }
        if (n > (uint32_t)(stop - o)) n = (uint32_t)(stop - o);
        memset(o, (int)value, 16); /* one fixed store for a short run: the next run writes over the rest */
        if (n > 16) memset(o + 16, (int)value, n - 16);
        o += n;
    }
#undef NIBBLE
    return px;
}

void ink_font_alpha(const ink_font_t *f, const ink_glyph_t *g, uint8_t *out, size_t stride) {
    uint8_t dark[256];
    int levels = (1 << f->bpp) - 1;
    for (int v = 0; v <= levels; v++) dark[v] = ink_font_darken(v * 255 / levels);
    const uint8_t *px = ink_font_pixels(f, g);
    for (int y = 0; y < g->h; y++)
        for (int x = 0; x < g->w; x++) out[(size_t)y * stride + x] = dark[*px++];
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
    osal_cost_units("glyph.blit", 1);
    ink_font_alpha(f, &g, draw_buf->data, lv_draw_buf_width_to_stride(g.w, LV_COLOR_FORMAT_A8));
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

static const int SIZES[] = {44}; /* generated by CMake (tools/fontgen) */
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
