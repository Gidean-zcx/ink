/* Text kernels on the synthetic Chinese sample (kernels/data/text_zh.txt,
 * ~6000 code points, loaded into PSRAM like a chapter read from storage).
 *
 *   text.break_char  : UTF-8 decode + advance width + greedy line breaking with
 *                      CJK rules (break between ideographs, keep Latin words
 *                      together, no line-start/line-end punctuation), per char
 *   text.layout_char : the above plus a glyph-cache lookup (hash table in PSRAM)
 *                      and emitting a positioned draw list, per char */
#include "k.h"

#define LINE_W 636 /* 684 px minus 2 x 24 px margins */
#define EM 24

static const uint8_t ascii_adv[128] = {
    /* proportional Latin advances at 24 px em (control chars 0) */
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  6,  7,  9,  13, 13, 18, 15, 5,  8,  8,  9,  13,
    6,  8,  6,  7,  13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 6,  6,  13, 13, 13, 12, 22, 15,
    15, 16, 16, 15, 14, 17, 16, 6,  11, 15, 13, 19, 16, 17, 15, 17, 16, 15, 14, 16, 15, 21,
    15, 15, 14, 7,  7,  7,  11, 12, 8,  12, 13, 12, 13, 12, 7,  13, 13, 5,  5,  11, 5,  19,
    13, 13, 13, 13, 8,  12, 7,  13, 11, 17, 11, 11, 11, 8,  6,  8,  13, 0};

static inline uint32_t utf8_next(const uint8_t **pp, const uint8_t *end) {
    const uint8_t *p = *pp;
    uint32_t c = *p++;
    if (c >= 0x80) {
        if (c < 0xE0 && p < end) {
            c = ((c & 0x1F) << 6) | (*p++ & 0x3F);
        } else if (c < 0xF0 && p + 1 < end) {
            c = ((c & 0x0F) << 12) | ((uint32_t)(p[0] & 0x3F) << 6) | (p[1] & 0x3F);
            p += 2;
        } else if (p + 2 < end) {
            c = ((c & 0x07) << 18) | ((uint32_t)(p[0] & 0x3F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            p += 3;
        } else {
            c = 0xFFFD;
        }
    }
    *pp = p;
    return c;
}

static inline int is_wide(uint32_t c) {
    return (c >= 0x2E80 && c <= 0x9FFF) || (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFF00 && c <= 0xFF60) ||
           (c >= 0x3000 && c <= 0x303F) || (c >= 0x2010 && c <= 0x2027);
}

static inline int char_width(uint32_t c) {
    if (c < 0x80) return ascii_adv[c];
    return is_wide(c) ? EM : EM / 2;
}

/* Kinsoku: may not start a line ... */
static int no_start(uint32_t c) {
    switch (c) {
    case 0xFF0C: case 0x3002: case 0x3001: case 0xFF1B: case 0xFF1A: case 0xFF1F: case 0xFF01: case 0xFF09:
    case 0x300D: case 0x300F: case 0x3011: case 0x300B: case 0x2026: case 0x2014: case 0x201D: case 0x2019:
    case ',': case '.': case ';': case ':': case '?': case '!': case ')':
        return 1;
    default:
        return 0;
    }
}
/* ... or end a line. */
static int no_end(uint32_t c) {
    switch (c) {
    case 0xFF08: case 0x300C: case 0x300E: case 0x3010: case 0x300A: case 0x201C: case 0x2018: case '(':
        return 1;
    default:
        return 0;
    }
}

typedef struct {
    uint32_t start, end; /* byte offsets */
    uint16_t width, chars;
} line_t;

typedef struct {
    int16_t x, y;
    uint16_t glyph;
} draw_t;

/* glyph cache: open-addressing hash, code point -> glyph slot */
#define GC_SLOTS 2048
typedef struct {
    uint32_t cp;
    uint16_t slot, adv;
} gc_entry_t;
static gc_entry_t *gcache;

static inline uint32_t gc_hash(uint32_t cp) { return (cp * 2654435761u) >> 21; }

static uint16_t gc_lookup(uint32_t cp, uint16_t *adv) {
    uint32_t h = gc_hash(cp) & (GC_SLOTS - 1);
    for (;;) {
        gc_entry_t *e = &gcache[h];
        if (e->cp == cp) {
            *adv = e->adv;
            return e->slot;
        }
        if (e->cp == 0) { /* miss: the rasterizer would run here; not part of layout */
            *adv = (uint16_t)char_width(cp);
            return 0xFFFF;
        }
        h = (h + 1) & (GC_SLOTS - 1);
    }
}

static void gc_insert(uint32_t cp, uint16_t slot) {
    uint32_t h = gc_hash(cp) & (GC_SLOTS - 1);
    while (gcache[h].cp && gcache[h].cp != cp) h = (h + 1) & (GC_SLOTS - 1);
    gcache[h].cp = cp;
    gcache[h].slot = slot;
    gcache[h].adv = (uint16_t)char_width(cp);
}

/* Greedy breaking. If draw != NULL also lays out glyphs (layout mode).
 * Returns the number of code points processed. */
static uint32_t layout(const uint8_t *text, size_t len, line_t *lines, int max_lines, int *nlines, draw_t *draw,
                       uint32_t *ndraw) {
    const uint8_t *p = text, *end = text + len;
    uint32_t chars = 0, nd = 0;
    int nl = 0, x = 0, brk_x = 0, line_chars = 0, brk_chars = 0;
    uint32_t line_start = 0, brk = 0;
    uint32_t prev = '\n';
    while (p < end) {
        uint32_t off = (uint32_t)(p - text);
        uint32_t c = utf8_next(&p, end);
        chars++;
        if (c == '\n') {
            if (nl < max_lines) lines[nl] = (line_t){line_start, off, (uint16_t)x, (uint16_t)line_chars};
            nl++;
            line_start = (uint32_t)(p - text);
            x = brk_x = line_chars = brk_chars = 0;
            brk = line_start;
            prev = c;
            continue;
        }
        int w;
        uint16_t slot = 0;
        if (draw) {
            uint16_t adv;
            slot = gc_lookup(c, &adv);
            w = adv;
        } else {
            w = char_width(c);
        }
        /* break opportunity before c? */
        if (!no_start(c) && !no_end(prev) && (is_wide(c) || is_wide(prev) || prev == ' ')) {
            brk = off;
            brk_x = x;
            brk_chars = line_chars;
        }
        if (x + w > LINE_W && x > 0) {
            uint32_t cut = brk > line_start ? brk : off;
            int cut_x = brk > line_start ? brk_x : x;
            int cut_chars = brk > line_start ? brk_chars : line_chars;
            if (nl < max_lines) lines[nl] = (line_t){line_start, cut, (uint16_t)cut_x, (uint16_t)cut_chars};
            nl++;
            if (draw) { /* re-position glyphs carried over to the new line */
                int carried = line_chars - cut_chars;
                for (int k = 0; k < carried; k++) {
                    draw_t *d = &draw[nd - carried + k];
                    d->x = (int16_t)(d->x - cut_x);
                    d->y = (int16_t)(nl * 30);
                }
            }
            line_start = cut;
            x -= cut_x;
            line_chars -= cut_chars;
            brk = line_start;
            brk_x = brk_chars = 0;
        }
        if (draw) draw[nd++] = (draw_t){(int16_t)x, (int16_t)(nl * 30), slot};
        x += w;
        line_chars++;
        prev = c;
    }
    *nlines = nl;
    if (ndraw) *ndraw = nd;
    return chars;
}

void k_text(void) {
    iss_alloc_reset();
    size_t len = (size_t)(k_text_zh_end - k_text_zh);
    uint8_t *text = iss_psram_alloc(len + 1, 64);
    memcpy(text, k_text_zh, len);
    text[len] = 0;
    line_t *lines = iss_sram_alloc(1024 * sizeof(line_t), 8);
    draw_t *draw = iss_psram_alloc(8192 * sizeof(draw_t), 8);
    gcache = iss_psram_alloc(GC_SLOTS * sizeof(gc_entry_t), 64);
    memset(gcache, 0, GC_SLOTS * sizeof(gc_entry_t));

    /* warm glyph cache with every code point of the text (a steady-state reader) */
    {
        const uint8_t *p = text, *end = text + len;
        uint16_t slot = 0;
        while (p < end) {
            uint32_t c = utf8_next(&p, end);
            uint16_t adv;
            if (c != '\n' && gc_lookup(c, &adv) == 0xFFFF) gc_insert(c, slot++);
        }
    }

    int nl;
    uint32_t chars, nd;
    layout(text, len, lines, 1024, &nl, NULL, NULL); /* warm-up */
    iss_begin("text.break_char");
    chars = layout(text, len, lines, 1024, &nl, NULL, NULL);
    iss_end(chars);

    layout(text, len, lines, 1024, &nl, draw, &nd); /* warm-up */
    iss_begin("text.layout_char");
    chars = layout(text, len, lines, 1024, &nl, draw, &nd);
    iss_end(chars);

    iss_cache_cold();
    iss_begin("text.layout_char#cold");
    chars = layout(text, len / 4, lines, 1024, &nl, draw, &nd);
    iss_end(chars);

    k_sink = (uint32_t)nl ^ nd ^ lines[3].end;
}
