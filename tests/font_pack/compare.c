/* Every glyph of a compressed face (INKF v2) unpacks, through system/text/ink_font.c, to exactly the bitmap
 * fontgen rendered for it (the same face written uncompressed, v1), and the old format still reads the same.
 * Usage: test-font-pack <v2.inkf> <v1.inkf> [...pairs]; tests/font_pack/run.py passes every shipped face. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_assets.h"
#include "ink/osal.h"
#include "ink/sys/font.h"

static size_t accessed;
static const uint8_t *memory; /* the asset "<memory>": a face with a damaged header (headers()) */
static size_t memory_size;
const void *hal_asset_map(const char *name, size_t *size) {
    if (!strcmp(name, "<memory>")) {
        *size = memory_size;
        return memory;
    }
    FILE *f = fopen(name, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(d); return NULL; }
    fclose(f);
    *size = (size_t)n;
    return d;
}
void hal_asset_unmap(const void *data) { (void)data; }
void hal_asset_access(const void *p, size_t len) { (void)p; accessed += len; }
void *osal_calloc(size_t n, size_t size, uint32_t caps) { (void)caps; return calloc(n, size); }
void *osal_malloc(size_t size, uint32_t caps) { (void)caps; return malloc(size); } /* LVGL's allocator, linked in */
void *osal_realloc(void *p, size_t size, uint32_t caps) { (void)caps; return realloc(p, size); }
void osal_mem_stats(uint32_t caps, osal_mem_stats_t *out) { (void)caps; memset(out, 0, sizeof *out); }
void osal_free(void *p) { free(p); }
const char *osal_mem_tag(const char *tag) { return tag; }
void osal_cost_units(const char *cost_class, uint64_t units) { (void)cost_class; (void)units; }

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int failures;
#define FAIL(...) do { if (failures++ < 10) { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static void compare(const char *packed_path, const char *raw_path) {
    size_t size = 0;
    const uint8_t *raw = hal_asset_map(raw_path, &size);
    ink_font_t *v2 = ink_font_open(packed_path), *v1 = ink_font_open(raw_path);
    if (!raw || !v2 || !v1 || size < 40 || raw[4] != 1 || raw[6] != 2) {
        FAIL("%s / %s: cannot open both faces (v2, and v1 at 2 bpp)", packed_path, raw_path);
        return;
    }
    size_t n_packed = 0;
    const uint8_t *packed = hal_asset_map(packed_path, &n_packed);
    if (!packed || n_packed < 40 || packed[4] != 2 || rd32(packed + 16) != rd32(raw + 16))
        FAIL("%s: not INKF v2 with the same glyphs as %s", packed_path, raw_path);
    if (ink_font_px(v2) != ink_font_px(v1) || ink_font_bpp(v2) != 2 || ink_font_ascent(v2) != ink_font_ascent(v1) ||
        ink_font_descent(v2) != ink_font_descent(v1) || ink_font_line_height(v2) != ink_font_line_height(v1))
        FAIL("%s: metrics differ from %s", packed_path, raw_path);
    uint32_t n = rd32(raw + 16), index = rd32(raw + 20), bitmaps = rd32(raw + 24), glyphs = 0;
    int failed_before = failures;
    size_t stored = 0;
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *e = raw + index + 16u * i;
        uint32_t cp = rd32(e);
        int w = e[8], h = e[9], row_bytes = (w * 2 + 7) / 8;
        const uint8_t *bits = raw + bitmaps + rd32(e + 4);
        ink_glyph_t a, b;
        if (!ink_font_has(v2, cp) || !ink_font_glyph(v2, cp, &a) || !ink_font_glyph(v1, cp, &b) || a.cp != cp ||
            b.cp != cp || a.w != w || a.h != h || a.x_ofs != (int8_t)e[10] || a.y_ofs != (int8_t)e[11] ||
            a.adv != e[12] || b.w != w || b.h != h || b.x_ofs != a.x_ofs || b.y_ofs != a.y_ofs || b.adv != a.adv) {
            FAIL("%s U+%04X: glyph entry differs", packed_path, (unsigned)cp);
            continue;
        }
        glyphs++;
        if (!w || !h) continue;
        static uint8_t got[255 * 255], want[255 * 255];
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) want[y * w + x] = (uint8_t)((bits[y * row_bytes + x / 4] >> (6 - 2 * (x & 3))) & 3);
        size_t before = accessed, area = (size_t)w * h;
        memcpy(got, ink_font_pixels(v2, &a), area);
        if (memcmp(got, want, area)) FAIL("%s U+%04X: unpacked pixels differ", packed_path, (unsigned)cp);
        if (memcmp(ink_font_pixels(v1, &b), want, area)) FAIL("%s U+%04X: v1 pixels differ", raw_path, (unsigned)cp);
        stored += a.stored;
        if (b.stored != h * row_bytes || accessed - before != (size_t)a.stored + b.stored)
            FAIL("%s U+%04X: %zu bytes charged to the flash cache for %u + %u stored", packed_path, (unsigned)cp,
                 accessed - before, a.stored, b.stored);
        /* The stream ends where the pixels do: nothing left over, nothing missing (a last odd nibble is padding, 0). */
        size_t nibbles = 0, pixels = 0;
        while (pixels < area && nibbles < 2u * a.stored) {
            unsigned code = (a.bits[nibbles / 2] >> (nibbles % 2 ? 0 : 4)) & 15, k = 15;
            nibbles++;
            size_t run = code & 3;
            for (run = run ? run : 4; !(code & 3) && k == 15 && nibbles < 2u * a.stored; nibbles++)
                run += k = (a.bits[nibbles / 2] >> (nibbles % 2 ? 0 : 4)) & 15;
            pixels += run;
        }
        if (pixels != area || (nibbles != 2u * a.stored && !(nibbles + 1 == 2u * a.stored && !(a.bits[a.stored - 1] & 15))))
            FAIL("%s U+%04X: %zu pixels in %zu of %u nibbles", packed_path, (unsigned)cp, pixels, nibbles, 2u * a.stored);
        static uint8_t alpha_a[255 * 260], alpha_b[255 * 260];
        size_t stride = (size_t)w + 3;
        ink_font_alpha(v2, &a, alpha_a, stride);
        ink_font_alpha(v1, &b, alpha_b, stride);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int v = (bits[y * row_bytes + x / 4] >> (6 - 2 * (x & 3))) & 3;
                if (alpha_a[y * stride + x] != ink_font_darken(v * 255 / 3) || alpha_b[y * stride + x] != alpha_a[y * stride + x])
                    FAIL("%s U+%04X (%d,%d): A8 differs", packed_path, (unsigned)cp, x, y);
            }
    }
    printf("%s: %u glyphs %s %s; bitmaps %zu bytes, uncompressed %u\n", packed_path, (unsigned)glyphs,
           failures == failed_before ? "identical to" : "NOT all identical to", raw_path, stored, (unsigned)rd32(raw + 28));
}

/* A header with a bpp the format does not hold must not open: v1 holds 1, 2, 4 or 8 (the coverage tables in
 * ink_font_alpha() and the reader's blit have 256 entries), v2 only 2. A face that does open draws every glyph
 * inside its buffers (build-asan checks that). */
static void headers(const char *packed_path, const char *raw_path) {
    const char *paths[2] = {raw_path, packed_path};
    for (int version = 1; version <= 2; version++) {
        size_t size = 0;
        const uint8_t *face = hal_asset_map(paths[version - 1], &size);
        uint8_t *copy = face && size >= 40 ? malloc(size) : NULL;
        if (!copy || face[4] != version) {
            FAIL("%s: cannot read an INKF v%d face", paths[version - 1], version);
            free(copy);
            continue;
        }
        memcpy(copy, face, size);
        memory = copy;
        memory_size = size;
        uint32_t n = rd32(copy + 16), index = rd32(copy + 20);
        for (int bpp = 0; bpp < 256; bpp++) {
            copy[6] = (uint8_t)bpp;
            ink_font_t *f = ink_font_open("<memory>");
            bool valid = bpp == 2 || (version == 1 && (bpp == 1 || bpp == 4 || bpp == 8));
            if (!f != !valid) FAIL("INKF v%d at %d bpp: %s", version, bpp, f ? "opened" : "refused");
            if (!f) continue;
            for (uint32_t i = 0; i < n && index + 16ull * (i + 1) <= size; i++) {
                static uint8_t alpha[255 * 255];
                ink_glyph_t g;
                if (ink_font_glyph(f, rd32(copy + index + 16u * i), &g) && g.w && g.h) ink_font_alpha(f, &g, alpha, g.w);
            }
            free(f); /* fonts are never freed by the product; this test's osal_calloc() is calloc() */
        }
        free(copy);
    }
    memory = NULL;
}

int main(int argc, char **argv) {
    if (argc < 3 || argc % 2 == 0) {
        fprintf(stderr, "usage: %s <v2.inkf> <v1.inkf> [...]\n", argv[0]);
        return 2;
    }
    headers(argv[1], argv[2]);
    for (int i = 1; i + 1 < argc; i += 2) compare(argv[i], argv[i + 1]);
    if (failures) fprintf(stderr, "%d differences\n", failures);
    return failures ? 1 : 0;
}
