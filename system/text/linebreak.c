/*
 * Line breaking rules (simplified UAX #14 + Chinese 禁则, see W3C "Requirements
 * for Chinese Text Layout" clreq §3.1.7):
 *  - never start a line with closing punctuation / small kana-like marks
 *  - never end a line with opening punctuation
 *  - keep Latin words and numbers together; break after spaces
 *  - break freely between Han characters and between Han and Latin
 *  - keep "……" and "——" pairs together
 */
#include "ink/sys/text.h"

uint32_t ink_utf8_next(const char *s, size_t len, size_t *i) {
    const unsigned char *p = (const unsigned char *)s;
    size_t k = *i;
    if (k >= len) return 0;
    uint32_t c = p[k];
    if (c < 0x80) {
        *i = k + 1;
        return c;
    }
    int n = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
    if (n < 0 || k + (size_t)n >= len) {
        *i = k + 1;
        return 0xFFFD;
    }
    c &= (uint32_t)(0x3F >> n);
    for (int j = 1; j <= n; j++) {
        unsigned char b = p[k + (size_t)j];
        if ((b & 0xC0) != 0x80) {
            *i = k + 1;
            return 0xFFFD;
        }
        c = (c << 6) | (b & 0x3F);
    }
    *i = k + (size_t)n + 1;
    /* overlong forms, UTF-16 surrogates and values past U+10FFFF are not characters */
    static const uint32_t MIN[4] = {0, 0x80, 0x800, 0x10000};
    if (c < MIN[n] || (c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF) return 0xFFFD;
    return c;
}

static bool in(uint32_t c, const char *set) {
    size_t i = 0, n = 0;
    while (set[n]) n++;
    while (i < n) {
        if (ink_utf8_next(set, n, &i) == c) return true;
    }
    return false;
}

bool ink_lb_no_break_before(uint32_t c) {
    if (c < 0x80) return c == ',' || c == '.' || c == ';' || c == ':' || c == '?' || c == '!' || c == ')' || c == ']' ||
                         c == '}' || c == '%' || c == '\'' || c == '"';
    return in(c, "，。、；：？！）】」』〉》〕〗〙〛｝］’”…—·・％‰℃～｡､﹐﹒﹔﹕﹖﹗");
}

bool ink_lb_no_break_after(uint32_t c) {
    if (c < 0x80) return c == '(' || c == '[' || c == '{';
    return in(c, "（【「『〈《〔〖〘〚｛［‘“");
}

static bool is_word(uint32_t c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= 0xC0 && c < 0x250) ||
           c == '-' || c == '\'' || c == '_';
}

bool ink_lb_can_break(uint32_t a, uint32_t b) {
    if (!a) return true;
    if (a == ' ') return b != ' ' && !ink_lb_no_break_before(b);
    if (b == ' ') return false; /* break after the space, not before */
    if (ink_lb_no_break_before(b) || ink_lb_no_break_after(a)) return false;
    if ((a == 0x2026 && b == 0x2026) || (a == 0x2014 && b == 0x2014)) return false;
    if (is_word(a) && is_word(b)) return false;
    return true;
}
