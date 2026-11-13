/* font-pack-1: what unpacking a glyph costs on the ESP32-S31, measured on tools/iss (fitted S31 model).
 * The same glyphs (the start of the test book) are drawn three ways, each before (the INKF v1 face, the drawing loops
 * as they were up to fe5938e3, copied below) and after (the v2 face, the product's system/text/ink_font.c and the
 * loops as they are now): the reader's page blit (system/text/layout.c), LVGL's A8 at the face's own size
 * (ink_font_alpha) and the theme's scaled path at 38 px (system/ui/theme.c). Faces are in flash, cold at the start
 * of each region (a refresh evicts them), code hot. Every output is compared byte for byte, before against after.
 * Built and run by run.sh. */
#include <stdio.h>
#include <string.h>

#ifdef __riscv
#include "iss_rt.h"
#else /* the same program on the build machine (run.sh host): best of HOST_RUNS passes, nanoseconds per glyph */
#include <stdlib.h>
#include <time.h>
enum { HOST_RUNS = 50 };
static const char *region;
static uint64_t started, best[16];
static const char *names[16];
static uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
}
static void iss_cache_cold(void) {}
static void iss_begin(const char *name) { region = name; started = now_ns(); }
static void iss_end2(uint32_t units, uint32_t aux) {
    uint64_t ns = (now_ns() - started) / units;
    (void)aux;
    for (int i = 0; i < 16; i++)
        if (!names[i] || names[i] == region) {
            names[i] = region;
            if (!best[i] || ns < best[i]) best[i] = ns;
            break;
        }
}
static void iss_end(uint32_t units) { iss_end2(units, 0); }
static void *iss_psram_alloc(size_t n, size_t align) { (void)align; return malloc(n); }
#endif
#include "ink/hal_assets.h"
#include "ink/osal.h"
#include "ink/sys/font.h"

extern const uint8_t v1_face[], v1_face_end[], v2_face[], v2_face_end[], book[], book_end[];
const void *hal_asset_map(const char *name, size_t *size) {
    bool v1 = !strcmp(name, "v1");
    *size = (size_t)(v1 ? v1_face_end - v1_face : v2_face_end - v2_face);
    return v1 ? v1_face : v2_face;
}
void hal_asset_unmap(const void *data) { (void)data; }
void hal_asset_access(const void *p, size_t len) { (void)p; (void)len; }
void *osal_calloc(size_t n, size_t size, uint32_t caps) {
    (void)caps;
    void *p = iss_psram_alloc(n * size, 8);
    memset(p, 0, n * size);
    return p;
}
void osal_free(void *p) { (void)p; }
const char *osal_mem_tag(const char *tag) { return tag; }
void osal_cost_units(const char *cost_class, uint64_t units) { (void)cost_class; (void)units; }
uint32_t lv_draw_buf_width_to_stride(uint32_t w, lv_color_format_t color_format) { (void)color_format; return w; }
#include "../../../../system/text/ink_font.c"

/* ---- before: fe5938e3 */
static void blit_v1(const ink_font_t *f, const ink_glyph_t *g, uint8_t *buf, int stride, int bw, int bh, int x0, int y0) {
    int bpp = ink_font_bpp(f), levels = (1 << bpp) - 1;
    int row_bytes = (g->w * bpp + 7) / 8;
    for (int y = 0; y < g->h; y++) {
        int by = y0 + y;
        if (by < 0 || by >= bh) continue;
        const uint8_t *row = g->bits + (size_t)y * row_bytes;
        uint8_t *dst = buf + (size_t)by * stride;
        for (int x = 0; x < g->w; x++) {
            int bx = x0 + x;
            if (bx < 0 || bx >= bw) continue;
            int bit = x * bpp;
            int a = (row[bit >> 3] >> (8 - bpp - (bit & 7))) & levels;
            if (!a) continue;
            int v = 255 - ink_font_darken(a * 255 / levels); /* black ink over white paper */
            if (v < dst[bx]) dst[bx] = (uint8_t)v;
        }
    }
}
static int coverage(const uint8_t *bits, int row_bytes, int bpp, int x, int y) {
    int bit = x * bpp;
    return (bits[y * row_bytes + (bit >> 3)] >> (8 - bpp - (bit & 7))) & ((1 << bpp) - 1);
}
static void alpha_v1(const ink_font_t *f, const ink_glyph_t *g, uint8_t *out, unsigned stride) {
    int bpp = ink_font_bpp(f), levels = (1 << bpp) - 1, row_bytes = (g->w * bpp + 7) / 8;
    for (int y = 0; y < g->h; y++)
        for (int x = 0; x < g->w; x++)
            out[(size_t)y * stride + x] = ink_font_darken(coverage(g->bits, row_bytes, bpp, x, y) * 255 / levels);
}
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
static void scaled(const ink_font_t *f, const ink_glyph_t *g, uint8_t *out, int px, bool v2) {
    int bpp = ink_font_bpp(f), levels = (1 << bpp) - 1, row_bytes = (g->w * bpp + 7) / 8, source_px = ink_font_px(f);
    int box_w = (g->w * px + source_px - 1) / source_px, box_h = (g->h * px + source_px - 1) / source_px;
    unsigned stride = (unsigned)box_w;
    const uint8_t *cover = v2 ? ink_font_pixels(f, g) : NULL;
    int window = source_px * 256 / px;
    if (window < 256) window = 256;
    for (int y = 0; y < box_h; y++) {
        int y0 = (2 * y + 1) * source_px * 128 / px - window / 2;
        for (int x = 0; x < box_w; x++) {
            int x0 = (2 * x + 1) * source_px * 128 / px - window / 2, sum = 0;
            for (int sy = y0 >> 8; sy <= (y0 + window - 1) >> 8; sy++) {
                if (sy < 0 || sy >= g->h) continue;
                int wy = MIN(y0 + window, (sy + 1) * 256) - MAX(y0, sy * 256);
                for (int sx = x0 >> 8; sx <= (x0 + window - 1) >> 8; sx++) {
                    if (sx < 0 || sx >= g->w) continue;
                    int wx = MIN(x0 + window, (sx + 1) * 256) - MAX(x0, sx * 256);
                    sum += (v2 ? cover[sy * g->w + sx] : coverage(g->bits, row_bytes, bpp, sx, sy)) * (wx * wy >> 8);
                }
            }
            out[(size_t)y * stride + x] = ink_font_darken(MIN(255, sum * 255 / levels / (window * window >> 8)));
        }
    }
}

/* The before loop with after's table instead of the division per pixel: after minus this is the unpacking alone. */
static void blit_v1_table(const ink_font_t *f, const ink_glyph_t *g, uint8_t *buf, int stride, int bw, int bh, int x0, int y0) {
    int bpp = ink_font_bpp(f), levels = (1 << bpp) - 1;
    int row_bytes = (g->w * bpp + 7) / 8;
    uint8_t ink[256];
    for (int a = 0; a <= levels; a++) ink[a] = (uint8_t)(255 - ink_font_darken(a * 255 / levels));
    for (int y = 0; y < g->h; y++) {
        int by = y0 + y;
        if (by < 0 || by >= bh) continue;
        const uint8_t *row = g->bits + (size_t)y * row_bytes;
        uint8_t *dst = buf + (size_t)by * stride;
        for (int x = 0; x < g->w; x++) {
            int bx = x0 + x;
            if (bx < 0 || bx >= bw) continue;
            int bit = x * bpp;
            int a = (row[bit >> 3] >> (8 - bpp - (bit & 7))) & levels;
            if (!a) continue;
            if (ink[a] < dst[bx]) dst[bx] = ink[a];
        }
    }
}

/* ---- after: the product's loop (system/text/layout.c) */
static void blit_v2(const ink_font_t *f, const ink_glyph_t *g, uint8_t *buf, int stride, int bw, int bh, int x0, int y0) {
    int levels = (1 << ink_font_bpp(f)) - 1;
    uint8_t ink[256]; /* ink[coverage]: black over white paper */
    for (int a = 0; a <= levels; a++) ink[a] = (uint8_t)(255 - ink_font_darken(a * 255 / levels));
    const uint8_t *px = ink_font_pixels(f, g);
    for (int y = 0; y < g->h; y++) {
        int by = y0 + y;
        const uint8_t *row = px + (size_t)y * g->w;
        if (by < 0 || by >= bh) continue;
        uint8_t *dst = buf + (size_t)by * stride;
        for (int x = 0; x < g->w; x++) {
            int bx = x0 + x;
            if (bx < 0 || bx >= bw) continue;
            int a = row[x];
            if (!a) continue;
            if (ink[a] < dst[bx]) dst[bx] = ink[a];
        }
    }
}

enum { W = 1000, H = 1400, MAX_GLYPHS = 600 };
static uint32_t cps[MAX_GLYPHS];
static int n;

static uint32_t sum(const uint8_t *p, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

typedef void blit_fn(const ink_font_t *, const ink_glyph_t *, uint8_t *, int, int, int, int, int);
static uint32_t reader(const ink_font_t *f, uint8_t *page, const char *name, blit_fn *blit, uint32_t *pixels) {
    memset(page, 0xff, (size_t)W * H);
    iss_cache_cold();
    iss_begin(name);
    int x = 40, y = 60;
    for (int i = 0; i < n; i++) {
        ink_glyph_t g;
        if (!ink_font_glyph(f, cps[i], &g)) continue;
        blit(f, &g, page, W, W, H, x + g.x_ofs, y + g.y_ofs);
        *pixels += (uint32_t)g.w * g.h;
        if ((x += g.adv) > W - 80) x = 40, y += 62;
    }
    iss_end2((uint32_t)n, *pixels);
    return sum(page, (size_t)W * H);
}

static uint32_t a8(const ink_font_t *f, uint8_t *out, bool v2) {
    uint32_t h = 0, pixels = 0;
    iss_cache_cold();
    iss_begin(v2 ? "a8.after" : "a8.before");
    for (int i = 0; i < n; i++) {
        ink_glyph_t g;
        if (!ink_font_glyph(f, cps[i], &g) || !g.w || !g.h) continue;
        if (v2) ink_font_alpha(f, &g, out, g.w);
        else alpha_v1(f, &g, out, g.w);
        pixels += (uint32_t)g.w * g.h;
        h = h * 31 + out[g.w * g.h / 2];
    }
    iss_end2((uint32_t)n, pixels);
    return h;
}

static uint32_t thirty_eight(const ink_font_t *f, uint8_t *out, bool v2) {
    uint32_t h = 0;
    iss_cache_cold();
    iss_begin(v2 ? "scaled38.after" : "scaled38.before");
    for (int i = 0; i < n; i++) {
        ink_glyph_t g;
        if (!ink_font_glyph(f, cps[i], &g) || !g.w || !g.h) continue;
        scaled(f, &g, out, 38, v2);
        h = h * 31 + sum(out, (size_t)((g.w * 38 + 43) / 44) * ((g.h * 38 + 43) / 44));
    }
    iss_end((uint32_t)n);
    return h;
}

static int once(void);
int main(void) {
#ifdef __riscv
    return once();
#else
    int r = 0;
    for (int i = 0; i < HOST_RUNS; i++) r |= once();
    for (int i = 0; i < 16 && names[i]; i++) printf("host %-16s %6llu ns/glyph\n", names[i], (unsigned long long)best[i]);
    return r;
#endif
}

static int once(void) {
    n = 0;
    const uint8_t *p = book;
    while (p < book_end && n < MAX_GLYPHS) { /* UTF-8: the CJK and punctuation the reader draws (no spaces, no breaks) */
        uint32_t c = *p++;
        if (c >= 0xe0 && p + 1 < book_end) c = (c & 0x0f) << 12 | (p[0] & 0x3fu) << 6 | (p[1] & 0x3fu), p += 2;
        else if (c >= 0xc0 && p < book_end) c = (c & 0x1f) << 6 | (*p++ & 0x3fu);
        if (c > 0x20 && c != 0x3000) cps[n++] = c;
    }
    static ink_font_t *v1, *v2;
    static uint8_t *page_a, *page_b, *out_a, *out_b;
    if (!v1) {
        v1 = ink_font_open("v1"), v2 = ink_font_open("v2");
        page_a = iss_psram_alloc((size_t)W * H, 64), page_b = iss_psram_alloc((size_t)W * H, 64);
        out_a = iss_psram_alloc(255 * 255, 64), out_b = iss_psram_alloc(255 * 255, 64);
    }
    if (!v1 || !v2 || !page_a || !page_b) return 2;
    uint32_t px_a = 0, px_b = 0, px_c = 0;
    uint32_t ra = reader(v1, page_a, "reader.before", blit_v1, &px_a), rb = reader(v2, page_b, "reader.after", blit_v2, &px_b);
    if (reader(v1, page_b, "reader.table", blit_v1_table, &px_c) != ra) ra = 0;
    uint32_t aa = a8(v1, out_a, false), ab = a8(v2, out_b, true);
    uint32_t sa = thirty_eight(v1, out_a, false), sb = thirty_eight(v2, out_b, true);
    printf("%d glyphs, %lu pixels: reader page %s, A8 %s, scaled %s\n", n, (unsigned long)px_a,
           ra == rb && !memcmp(page_a, page_b, (size_t)W * H) ? "identical" : "DIFFERENT", aa == ab ? "identical" : "DIFFERENT",
           sa == sb ? "identical" : "DIFFERENT");
    return ra == rb && aa == ab && sa == sb ? 0 : 1;
}
