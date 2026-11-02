/*
 * Page layout and rendering for body text.
 * Greedy line filling with break opportunities from linebreak.c; closing
 * punctuation that would start a line hangs into the margin instead (clreq
 * "hanging punctuation", push-in); full lines are justified across the
 * character gaps; paragraphs ('\n') get a first-line indent.
 */
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/text.h"

static int advance(const ink_font_t *f, uint32_t cp) {
    ink_glyph_t g;
    if (cp == 0x3000) return ink_font_px(f); /* ideographic space */
    return ink_font_glyph(f, cp, &g) ? g.adv : ink_font_px(f) / 2;
}

size_t ink_layout_page(const ink_layout_t *L, const char *text, size_t len, size_t start, ink_page_t *page) {
    const ink_font_t *f = L->font;
    int ascent = ink_font_ascent(f), line_h = ink_font_line_height(f) + L->line_gap;
    int px = ink_font_px(f);
    memset(page, 0, sizeof *page);
    page->start = (uint32_t)start;
    size_t i = start;
    int y = 0;
    uint64_t chars = 0;
    while (page->nlines < INK_MAX_LINES && i < len) {
        if (y + line_h > L->height) break;
        bool para_start = i == 0 || text[i - 1] == '\n';
        /* skip spaces that a break left at the start of a line; at a paragraph start also skip
         * the ideographic-space indent many .txt files carry (we indent ourselves) */
        for (;;) {
            size_t k = i;
            uint32_t c = i < len ? ink_utf8_next(text, len, &k) : 0;
            if (c == ' ' || c == '\t' || (para_start && c == 0x3000)) i = k;
            else break;
        }
        int indent = para_start ? L->indent_em * px : 0;
        int x = indent, nchars = 0;
        size_t j = i, line_end = len, next = len, last_break = 0;
        int last_break_n = 0;
        uint32_t prev = 0;
        bool para_end = false;
        while (j < len) {
            size_t k = j;
            uint32_t cp = ink_utf8_next(text, len, &k);
            chars++;
            if (cp == '\n') {
                line_end = j;
                next = k;
                para_end = true;
                break;
            }
            if (cp == '\r') {
                j = k;
                continue;
            }
            int adv = advance(f, cp);
            if (prev && ink_lb_can_break(prev, cp)) {
                last_break = j;
                last_break_n = nchars;
            }
            if (x + adv > L->width && nchars > 0) {
                size_t k3 = k;
                uint32_t after = k3 < len ? ink_utf8_next(text, len, &k3) : 0;
                if (ink_lb_no_break_before(cp) && !ink_lb_no_break_before(after)) {
                    /* hang one closing punctuation mark into the margin (push-in) */
                    line_end = next = k;
                    nchars++;
                } else if (last_break > i) { /* push the last breakable unit to the next line */
                    line_end = next = last_break;
                    nchars = last_break_n;
                } else {
                    line_end = next = j; /* no opportunity (very long word): break here */
                }
                break;
            }
            x += adv;
            nchars++;
            prev = cp;
            j = k;
        }
        if (j >= len && !para_end) {
            line_end = next = len;
            para_end = true;
        }
        ink_line_t *ln = &page->lines[page->nlines++];
        ln->start = (uint32_t)i;
        ln->end = (uint32_t)line_end;
        ln->y = (int16_t)(y + ascent);
        ln->indent = (int16_t)indent;
        ln->nchars = (uint16_t)nchars;
        ln->para_end = para_end;
        if (!para_end) {
            /* measure the final line content to justify it */
            int w = indent;
            size_t q = i;
            while (q < line_end) w += advance(f, ink_utf8_next(text, len, &q));
            ln->extra = (int16_t)(L->width - w > 0 ? L->width - w : 0);
        }
        y += line_h + (para_end ? L->para_gap : 0);
        i = next;
    }
    page->end = (uint32_t)i;
    osal_cost_units("text.layout_char", chars);
    return i;
}

static void blit(const ink_font_t *f, const ink_glyph_t *g, uint8_t *buf, int stride, int bw, int bh, int x0, int y0) {
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
            int v = 255 - a * 255 / levels; /* black ink over white paper */
            if (v < dst[bx]) dst[bx] = (uint8_t)v;
        }
    }
}

void ink_render_page(const ink_layout_t *L, const char *text, const ink_page_t *page, uint8_t *buf, int stride,
                     int buf_w, int buf_h, int ox, int oy) {
    const ink_font_t *f = L->font;
    size_t len = page->end;
    uint64_t glyphs = 0;
    for (int n = 0; n < page->nlines; n++) {
        const ink_line_t *ln = &page->lines[n];
        int gaps = ln->nchars > 1 ? ln->nchars - 1 : 0;
        int x = ox + ln->indent, gi = 0;
        size_t q = ln->start;
        while (q < ln->end && q < len + 4) {
            uint32_t cp = ink_utf8_next(text, ln->end, &q);
            if (cp == '\r' || cp == '\n') continue;
            ink_glyph_t g;
            if (cp != ' ' && cp != 0x3000 && ink_font_glyph(f, cp, &g)) {
                ink_font_touch_bitmap(f, &g);
                blit(f, &g, buf, stride, buf_w, buf_h, x + g.x_ofs, oy + ln->y + g.y_ofs);
                glyphs++;
            }
            x += advance(f, cp);
            if (!ln->para_end && gaps) x += ln->extra / gaps + (gi < ln->extra % gaps ? 1 : 0);
            gi++;
        }
        if ((n & 3) == 3) osal_preempt_point();
    }
    osal_cost_units("glyph.blit", glyphs);
}
