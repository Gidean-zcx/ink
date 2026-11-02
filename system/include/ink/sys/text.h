/*
 * Text layout for reading: UTF-8 decoding, line breaking with Chinese
 * line-start/line-end prohibitions (禁则) and unbroken Latin words, justified
 * CJK lines, paragraph indent, pagination and rendering into 8-bit buffers.
 * LVGL 9.5 treats every Han character as a word and has no 禁则, so body
 * text uses this engine (foundation §4.6); UI labels keep using LVGL.
 */
#ifndef INK_SYS_TEXT_H
#define INK_SYS_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ink/sys/font.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Decode one UTF-8 code point at s[*i] (advances *i). Invalid bytes yield U+FFFD. */
uint32_t ink_utf8_next(const char *s, size_t len, size_t *i);

bool ink_lb_no_break_before(uint32_t cp); /* line-start prohibited: closing punctuation ... */
bool ink_lb_no_break_after(uint32_t cp);  /* line-end prohibited: opening punctuation ... */
/** May a line break between a and b? */
bool ink_lb_can_break(uint32_t a, uint32_t b);

typedef struct {
    const ink_font_t *font;
    int width, height;  /* text box in pixels */
    int line_gap;       /* extra pixels between lines */
    int para_gap;       /* extra pixels between paragraphs */
    int indent_em;      /* first-line indent in ems (2 for Chinese prose) */
} ink_layout_t;

typedef struct {
    uint32_t start, end; /* byte range in the text */
    int16_t y;           /* baseline */
    int16_t indent;      /* first-line indent in px */
    int16_t extra;       /* justification: extra px distributed over the gaps */
    uint16_t nchars;
    bool para_end;       /* last line of a paragraph: not justified */
} ink_line_t;

#define INK_MAX_LINES 64

typedef struct {
    ink_line_t lines[INK_MAX_LINES];
    int nlines;
    uint32_t start, end; /* byte range covered by the page */
} ink_page_t;

/** Lay out one page starting at byte `start`. Returns the start of the next page (== len at the end). */
size_t ink_layout_page(const ink_layout_t *L, const char *text, size_t len, size_t start, ink_page_t *page);
/** Render a laid-out page (black on white) into an 8-bit buffer; (ox, oy) = text box origin. */
void ink_render_page(const ink_layout_t *L, const char *text, const ink_page_t *page, uint8_t *buf, int stride,
                     int buf_w, int buf_h, int ox, int oy);

#ifdef __cplusplus
}
#endif
#endif
