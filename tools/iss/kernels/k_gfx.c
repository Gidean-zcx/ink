/* Gray-8 graphics kernels: anti-aliased glyph blit, rectangle fill, and the
 * 8bpp -> 4bpp packing done when a frame is handed to the EPD driver.
 *
 * Geometry follows the device: portrait UI framebuffer 684 x 1216 (stride
 * 688) gray8 in PSRAM (836 KiB, far larger than the 64 KiB D-cache); text at
 * 24 px em (CJK cell 24 x 24, line pitch 30 px); a glyph cache of 384 alpha-8
 * bitmaps in PSRAM (216 KiB). */
#include "k.h"

#define FB_W 684
#define FB_H 1216
#define FB_STRIDE 688
#define EM 24
#define LINE_PITCH 30
#define N_GLYPHS 384
#define PER_LINE (FB_W / EM) /* 28 */
#define LINES (FB_H / LINE_PITCH) /* 40 */

typedef struct {
    uint8_t w, h;
    int8_t bx, by; /* bearing */
    uint32_t off;  /* offset in the bitmap pool */
} glyph_t;

static glyph_t *glyphs;
static uint8_t *glyph_pool;
/* Ink level read at run time: LVGL's blend code does not know the colour at
 * compile time (a constant 0 would let GCC drop a multiply). */
static volatile uint8_t k_ink = 0x10;

/* Synthetic CJK-like glyph: 4-10 horizontal/vertical strokes, 2 px wide,
 * anti-aliased edges. Coverage ends up ~35-45 % non-zero like real CJK. */
static void make_glyph(uint8_t *g, uint32_t *seed) {
    memset(g, 0, EM * EM);
    int strokes = 4 + (int)(k_rand(seed) % 7);
    for (int s = 0; s < strokes; s++) {
        uint32_t r = k_rand(seed);
        int horiz = r & 1;
        int a = 2 + (int)((r >> 1) % 18);       /* row (horiz) or column */
        int b0 = 1 + (int)((r >> 6) % 10);      /* start */
        int b1 = b0 + 6 + (int)((r >> 11) % 12); /* end */
        if (b1 > EM - 2) b1 = EM - 2;
        uint8_t edge = (uint8_t)(48 + ((r >> 16) % 160));
        for (int t = b0; t <= b1; t++) {
            for (int k = -1; k <= 2; k++) {
                int y = horiz ? a + k : t, x = horiz ? t : a + k;
                if (x < 0 || y < 0 || x >= EM || y >= EM) continue;
                uint8_t v = (k == 0 || k == 1) ? 255 : edge;
                if (g[y * EM + x] < v) g[y * EM + x] = v;
            }
        }
    }
}

static void build_font(void) {
    glyphs = iss_psram_alloc(N_GLYPHS * sizeof *glyphs, 64);
    glyph_pool = iss_psram_alloc(N_GLYPHS * EM * EM, 64);
    uint32_t seed = 0x1234567u;
    for (int i = 0; i < N_GLYPHS; i++) {
        glyphs[i] = (glyph_t){EM, EM, 0, 0, (uint32_t)i * EM * EM};
        if (i >= 88 && i < 96) glyphs[i].w = EM / 2; /* a few half-width (Latin/digit) glyphs, ~8 % of text */
        make_glyph(glyph_pool + glyphs[i].off, &seed);
        if (glyphs[i].w < EM) /* repack to the narrower width */
            for (int y = 0; y < EM; y++)
                memmove(glyph_pool + glyphs[i].off + y * glyphs[i].w, glyph_pool + glyphs[i].off + y * EM, glyphs[i].w);
    }
}

/* A8 coverage blended over L8 with ink colour fg (LVGL-style; 0 and 255 fast paths). */
static void blit_a8(uint8_t *fb, int stride, int x0, int y0, int clip_w, int clip_h, const glyph_t *gl, uint8_t fg) {
    int w = gl->w, h = gl->h;
    int x = x0 + gl->bx, y = y0 + gl->by;
    if (x >= clip_w || y >= clip_h || x + w <= 0 || y + h <= 0) return;
    const uint8_t *src = glyph_pool + gl->off;
    int sx = 0, sy = 0;
    if (x < 0) sx = -x, w += x, x = 0;
    if (y < 0) sy = -y, h += y, y = 0;
    if (x + w > clip_w) w = clip_w - x;
    if (y + h > clip_h) h = clip_h - y;
    const uint8_t *s = src + sy * gl->w + sx;
    uint8_t *d = fb + y * stride + x;
    for (int r = 0; r < h; r++, s += gl->w, d += stride) {
        for (int c = 0; c < w; c++) {
            unsigned a = s[c];
            if (a == 0) continue;
            if (a == 255) {
                d[c] = fg;
                continue;
            }
            unsigned v = fg * a + d[c] * (255u - a);
            d[c] = (uint8_t)((v + 1 + (v >> 8)) >> 8); /* v / 255 */
        }
    }
}

/* Lays out `lines` lines of text from a glyph-index stream into fb. Returns
 * glyph count; *px gets the number of glyph-cell pixels touched. */
static uint32_t render_text(uint8_t *fb, int stride, int clip_h, int lines, uint32_t *seed, uint32_t *px) {
    uint32_t n = 0, pix = 0;
    for (int l = 0; l < lines; l++) {
        int x = 0, y = l * LINE_PITCH + 3;
        while (x + EM <= FB_W) {
            uint32_t r = k_rand(seed);
            /* Zipf-ish reuse: frequent characters come from the first slots */
            uint32_t idx = (r & 3) ? (r >> 8) % 96 : (r >> 8) % N_GLYPHS;
            const glyph_t *gl = &glyphs[idx];
            blit_a8(fb, stride, x, y, FB_W, clip_h, gl, k_ink);
            x += gl->w;
            pix += (uint32_t)gl->w * gl->h;
            n++;
        }
    }
    *px = pix;
    return n;
}

static void fill_rect(uint8_t *fb, int stride, int x, int y, int w, int h, uint8_t c) {
    uint8_t *p = fb + y * stride + x;
    for (int r = 0; r < h; r++, p += stride) memset(p, c, (size_t)w);
}

/* A UI-like mix of rectangles (bars, list rows, buttons, small icons). */
static uint32_t fill_ui(uint8_t *fb, int stride, int height) {
    static const struct {
        int16_t x, y, w, h;
    } r[] = {{0, 0, 684, 56},    {0, 56, 684, 120},  {16, 200, 652, 96},  {16, 320, 652, 96}, {16, 440, 200, 64},
             {240, 440, 200, 64}, {464, 440, 200, 64}, {24, 560, 48, 48},  {96, 560, 48, 48}, {168, 560, 48, 48},
             {0, 640, 684, 2},   {0, 700, 684, 200}, {32, 940, 300, 40}, {0, 1150, 684, 66}};
    uint32_t px = 0;
    for (unsigned i = 0; i < sizeof r / sizeof r[0]; i++) {
        if (r[i].y + r[i].h > height) continue;
        fill_rect(fb, stride, r[i].x, r[i].y, r[i].w, r[i].h, (uint8_t)(0x20 + 16 * i));
        px += (uint32_t)r[i].w * r[i].h;
    }
    return px;
}

/* 8bpp gray -> packed 4bpp, two pixels per byte (low nibble = left pixel). */
static void pack_4bpp(uint8_t *dst, const uint8_t *src, int w, int h, int src_stride) {
    for (int y = 0; y < h; y++) {
        const uint8_t *s = src + y * src_stride;
        uint8_t *d = dst + y * (w / 2);
        for (int x = 0; x < w; x += 2) d[x >> 1] = (uint8_t)((s[x] >> 4) | (s[x + 1] & 0xF0));
    }
}

void k_gfx(void) {
    iss_alloc_reset();
    build_font();
    uint8_t *fb = iss_psram_alloc(FB_STRIDE * FB_H, 64);
    memset(fb, 0xff, FB_STRIDE * FB_H);

    uint32_t seed, n, px;
    /* --- glyph blit into the PSRAM framebuffer: one full page --- */
    seed = 7;
    render_text(fb, FB_STRIDE, FB_H, LINES, &seed, &px); /* warm-up page */
    iss_cache(ISS_CACHE_WB);
    iss_begin("glyph.blit@psram");
    n = render_text(fb, FB_STRIDE, FB_H, LINES, &seed, &px);
    iss_cache_drain();
    iss_end2(n, px);

    /* --- same into an internal-SRAM band buffer (4 text lines, 82 KiB) --- */
    int band_h = 4 * LINE_PITCH;
    uint8_t *band = iss_sram_alloc(FB_STRIDE * band_h, 64);
    memset(band, 0xff, FB_STRIDE * band_h);
    render_text(band, FB_STRIDE, band_h, 4, &seed, &px);
    iss_begin("glyph.blit@sram");
    uint32_t tot = 0, totpx = 0;
    for (int k = 0; k < 10; k++) {
        tot += render_text(band, FB_STRIDE, band_h, 4, &seed, &px);
        totpx += px;
    }
    iss_end2(tot, totpx);

    /* --- cold start: empty caches, code fetched from flash --- */
    iss_cache_cold();
    iss_begin("glyph.blit@psram#cold");
    n = render_text(fb, FB_STRIDE, FB_H, 4, &seed, &px);
    iss_cache_drain();
    iss_end2(n, px);

    /* --- rectangle fill --- */
    fill_ui(fb, FB_STRIDE, FB_H);
    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    iss_begin("fill.px@psram");
    px = fill_ui(fb, FB_STRIDE, FB_H);
    iss_cache_drain();
    iss_end(px);

    iss_begin("fill.px@sram");
    px = 0;
    for (int k = 0; k < 4; k++) px += fill_ui(band, FB_STRIDE, band_h);
    iss_end(px);

    /* --- 8bpp -> 4bpp packing (panel orientation 1216 x 684) --- */
    const int PW = 1216, PH = 684;
    uint8_t *src_s = iss_sram_alloc(PW * 32, 64), *dst_s = iss_sram_alloc(PW * 16, 64);
    k_fill_random(src_s, PW * 32, 3);
    pack_4bpp(dst_s, src_s, PW, 32, PW);
    iss_begin("epd.convert_px@sram");
    for (int k = 0; k < 4; k++) pack_4bpp(dst_s, src_s, PW, 32, PW);
    iss_end(4 * PW * 32);

    uint8_t *dst_p = iss_psram_alloc(PW / 2 * PH, 64);
    iss_cache(ISS_CACHE_WB | ISS_CACHE_INV_D);
    iss_begin("epd.convert_px@psram");
    pack_4bpp(dst_p, fb, PW, PH, PW); /* fb reinterpreted as a 1216-wide frame */
    iss_cache_drain();
    iss_end(PW * PH);

    k_sink = fb[12345] ^ band[777] ^ dst_s[99] ^ dst_p[4321];
}
