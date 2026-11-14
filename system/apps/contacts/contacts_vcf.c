/* vCard reading and writing (ours). What ez-vcard does for Fossify's VcfImporter.kt:46-299 and VcfExporter.kt:32-219:
 * folded lines, QUOTED-PRINTABLE with soft breaks, CHARSET, item1. groups, 2.1 bare types, TYPE lists, quoted values,
 * tel: URIs and \n \, \; escapes; PHOTO and other long properties are stepped over, never copied. */
#include "contacts_vcf.h"
#include <stdio.h>
#include "ink/sys/app.h"
#include <string.h>

enum { LINE_MAX = 1024 };
typedef struct { ink_vcard_t card; char fn[INK_VC_NAME], n[INK_VC_NAME], org[INK_VC_NAME]; bool open, bad, noted; int dropped; } reading_t;

bool ink_vcard_number_ok(const char *s) {
    size_t n = strlen(s); bool digit = false;
    for (size_t k = 0; k < n; k++) {
        if (s[k] >= '0' && s[k] <= '9') digit = true;
        else if (!strchr(" -()", s[k]) && !(k == 0 && s[k] == '+')) return false;
    }
    return digit && n < INK_VC_NUMBER;
}
static int upper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
static bool same(const char *a, size_t n, const char *word) { /* a[0..n) is word, any case */
    if (strlen(word) != n) return false;
    for (size_t k = 0; k < n; k++) if (upper((unsigned char)a[k]) != word[k]) return false;
    return true;
}
static bool has(const char *a, size_t n, const char *word) { /* word appears in a[0..n), any case */
    for (size_t k = 0, w = strlen(word); k + w <= n; k++) if (same(a + k, w, word)) return true;
    return false;
}
static bool utf8_ok(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p;) {
        int more = *p < 0x80 ? 0 : (*p & 0xE0) == 0xC0 ? 1 : (*p & 0xF0) == 0xE0 ? 2 : (*p & 0xF8) == 0xF0 ? 3 : -1;
        if (more < 0 || (more == 1 && *p < 0xC2)) return false;
        for (p++; more > 0; more--, p++) if ((*p & 0xC0) != 0x80) return false;
    }
    return true;
}
void ink_vcard_cut(char *s, size_t chars) { /* at most chars characters, never half of one */
    size_t at = 0;
    for (size_t k = 0; s[at] && k < chars; k++) do at++; while ((s[at] & 0xC0) == 0x80);
    s[at] = 0;
}
static void whole(char *s) { /* drop a character cut short at the end */
    size_t n = strlen(s), at = n;
    while (at && (s[at - 1] & 0xC0) == 0x80) at--;
    unsigned char lead = at ? (unsigned char)s[at - 1] : 0;
    if (lead >= 0xC0 && n - (at - 1) < (size_t)(lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2)) s[at - 1] = 0;
}
static void copy_cut(char *dst, size_t cap, const char *src) { /* whole characters that fit */
    size_t n = strlen(src);
    if (n >= cap) { n = cap - 1; while (n && (src[n] & 0xC0) == 0x80) n--; }
    memcpy(dst, src, n); dst[n] = 0;
}
/* The property a physical line starts: the name without its group, up to ';' or ':'. */
static bool wanted(const char *s, const char *e) {
    const char *name = s, *p = s;
    while (p < e && *p != ';' && *p != ':') { if (*p == '.') name = p + 1; p++; }
    static const char *const KEEP[] = {"BEGIN", "END", "FN", "N", "ORG", "TEL", "NOTE"};
    for (size_t k = 0; k < sizeof KEEP / sizeof *KEEP; k++) if (same(name, (size_t)(p - name), KEEP[k])) return true;
    return false;
}
static const char *line_end(const char *p, const char *end) { const char *e = memchr(p, '\n', (size_t)(end - p)); return e ? e : end; }
/* The next logical line into buf ("" for a property we do not keep). False at the end of the text. */
static bool next_line(const char **at, const char *end, char *buf) {
    if (*at >= end) return false;
    const char *s = *at, *e = line_end(s, end);
    bool keep = wanted(s, e), qp = has(s, (size_t)(e - s), "QUOTED-PRINTABLE");
    size_t n = 0; buf[0] = 0;
    for (;;) {
        size_t len = (size_t)(e - s); if (len && s[len - 1] == '\r') len--;
        if (keep) { size_t take = n + len < LINE_MAX - 1 ? len : LINE_MAX - 1 - n; memcpy(buf + n, s, take); n += take; buf[n] = 0; }
        bool soft = qp && len && s[len - 1] == '=';
        *at = e < end ? e + 1 : end;
        if (*at >= end) break;
        if (soft) { if (keep && n) buf[--n] = 0; s = *at; e = line_end(s, end); continue; } /* =CRLF: the value goes on */
        if (**at == ' ' || **at == '\t') { s = *at + 1; e = line_end(s, end); continue; } /* folded */
        break;
    }
    return true;
}
static int hex(int c) { return c >= '0' && c <= '9' ? c - '0' : upper(c) >= 'A' && upper(c) <= 'F' ? upper(c) - 'A' + 10 : -1; }
static void qp_decode(char *s) {
    char *w = s;
    for (char *r = s; *r; r++) {
        if (*r == '=' && hex((unsigned char)r[1]) >= 0 && hex((unsigned char)r[2]) >= 0) { *w++ = (char)(hex((unsigned char)r[1]) * 16 + hex((unsigned char)r[2])); r += 2; }
        else *w++ = *r;
    }
    *w = 0;
}
/* One text component up to an unescaped ';' (or the end when all is true), \n \, \; \\ decoded. Returns where it stopped. */
static const char *component(const char *s, char *out, size_t cap, bool all) {
    size_t n = 0;
    for (; *s && (all || *s != ';'); s++) {
        char c = *s;
        if (c == '\\' && s[1]) { s++; c = *s == 'n' || *s == 'N' ? '\n' : *s; }
        if (n + 1 < cap) out[n++] = c;
    }
    out[n] = 0; whole(out);
    return *s ? s + 1 : s;
}
static void finish(reading_t *r, void (*fn)(void *, const ink_vcard_t *, ink_vcf_status_t, int), void *ctx) {
    ink_vcard_t *c = &r->card;
    copy_cut(c->name, sizeof c->name, r->fn[0] ? r->fn : r->n[0] ? r->n : r->org[0] ? r->org : c->count ? c->num[0] : "");
    ink_vcard_cut(c->name, 32);
    fn(ctx, c, r->bad ? INK_VCF_BAD : c->count ? INK_VCF_OK : INK_VCF_NO_NUMBER, r->dropped);
    memset(r, 0, sizeof *r);
}
static void tel(reading_t *r, const char *params, size_t plen, const char *value) {
    char clean[64]; size_t n = 0;
    if (!strncmp(value, "tel:", 4) || !strncmp(value, "TEL:", 4)) value += 4;
    for (const char *p = value; *p && *p != ';' && n + 1 < sizeof clean; p++) /* a tel: URI's ;ext= goes */
        if ((*p >= '0' && *p <= '9') || strchr(" -()", *p) || (*p == '+' && !n)) clean[n++] = *p;
    while (n && clean[n - 1] == ' ') n--;
    clean[n] = 0;
    char *number = clean; while (*number == ' ') number++;
    if (!ink_vcard_number_ok(number)) return;
    ink_vcard_t *c = &r->card;
    if (c->count == INK_VC_NUMBERS) { r->dropped++; return; }
    bool fax = has(params, plen, "FAX") || has(params, plen, "PAGER");
    c->type[c->count] = fax ? INK_VC_OTHER : has(params, plen, "CELL") ? INK_VC_MOBILE : has(params, plen, "HOME") ? INK_VC_HOME : has(params, plen, "WORK") ? INK_VC_WORK : INK_VC_OTHER;
    if (c->def < 0 && has(params, plen, "PREF")) c->def = (int8_t)c->count;
    copy_cut(c->num[c->count++], INK_VC_NUMBER, number);
}
void ink_vcf_parse(const char *text, size_t len, void (*fn)(void *ctx, const ink_vcard_t *card, ink_vcf_status_t status, int dropped), void *ctx) {
    static char line[LINE_MAX] INK_PSRAM_BSS;
    static reading_t r INK_PSRAM_BSS;
    const char *at = text, *end = text + len;
    memset(&r, 0, sizeof r);
    while (next_line(&at, end, line)) {
        if (!line[0]) continue;
        char *colon = line; bool quoted = false;
        while (*colon && (quoted || *colon != ':')) { if (*colon == '"') quoted = !quoted; colon++; }
        if (!*colon) { r.bad |= r.open; continue; }
        *colon = 0; char *value = colon + 1, *name = line, *params = strchr(line, ';');
        if (params) *params++ = 0; else params = colon;
        if (strrchr(name, '.')) name = strrchr(name, '.') + 1;
        size_t plen = strlen(params), nlen = strlen(name);
        if (same(name, nlen, "BEGIN") && same(value, strlen(value), "VCARD")) {
            if (r.open) { r.bad = true; finish(&r, fn, ctx); }
            r.open = true; r.card.def = -1; continue;
        }
        if (!r.open) continue;
        if (same(name, nlen, "END")) { finish(&r, fn, ctx); continue; }
        if (has(params, plen, "CHARSET=") && !has(params, plen, "CHARSET=UTF-8") && !has(params, plen, "CHARSET=US-ASCII")) { r.bad = true; continue; }
        if (has(params, plen, "QUOTED-PRINTABLE")) qp_decode(value);
        if (!utf8_ok(value)) { r.bad = true; continue; }
        if (same(name, nlen, "TEL")) tel(&r, params, plen, value);
        else if (same(name, nlen, "FN") && !r.fn[0]) component(value, r.fn, sizeof r.fn, true);
        else if (same(name, nlen, "ORG") && !r.org[0]) component(value, r.org, sizeof r.org, false);
        else if (same(name, nlen, "NOTE") && !r.noted) { component(value, r.card.note, sizeof r.card.note, true); ink_vcard_cut(r.card.note, 64); r.noted = true; }
        else if (same(name, nlen, "N") && !r.n[0]) { /* family;given: 张三 together, John Smith with a space */
            char family[INK_VC_NAME], given[INK_VC_NAME]; const char *rest = component(value, family, sizeof family, false);
            component(rest, given, sizeof given, false);
            bool wide = false; for (const char *p = family; *p; p++) wide |= (*p & 0x80) != 0;
            for (const char *p = given; *p; p++) wide |= (*p & 0x80) != 0;
            if (wide) snprintf(r.n, sizeof r.n, "%.48s%.48s", family, given);
            else snprintf(r.n, sizeof r.n, "%.47s%s%.48s", given, family[0] && given[0] ? " " : "", family);
            whole(r.n);
        }
    }
    if (r.open) { r.bad = true; finish(&r, fn, ctx); }
}

/* ---- writing ---- */
typedef struct { char *out; size_t cap, len; bool over; } out_t;
static void emit(out_t *o, const char *line) { /* one property, folded at 75 bytes between characters, CRLF */
    size_t n = strlen(line), at = 0; bool first = true;
    while (at < n || first) {
        size_t room = first ? 75 : 74, take = n - at < room ? n - at : room;
        while (at + take < n && take && (line[at + take] & 0xC0) == 0x80) take--;
        size_t need = (first ? 0 : 1) + take + 2;
        if (o->len + need >= o->cap) { o->over = true; return; }
        if (!first) o->out[o->len++] = ' ';
        memcpy(o->out + o->len, line + at, take); o->len += take; memcpy(o->out + o->len, "\r\n", 2); o->len += 2;
        at += take; first = false;
    }
    o->out[o->len] = 0;
}
static void escaped(char *dst, size_t cap, const char *head, const char *s, const char *tail) {
    size_t n = (size_t)snprintf(dst, cap, "%s", head);
    for (; *s && n + 3 < cap; s++) {
        if (*s == '\r') continue;
        if (*s == '\\' || *s == ',' || *s == ';' || *s == '\n') dst[n++] = '\\';
        dst[n++] = *s == '\n' ? 'n' : *s;
    }
    dst[n] = 0; if (n + strlen(tail) < cap) strcat(dst, tail);
}
int ink_vcf_write(char *out, size_t cap, const ink_vcard_t *c) {
    static const char *const TYPES[] = {"CELL", "HOME", "WORK", "VOICE"};
    char line[512]; out_t o = {out, cap, 0, false};
    emit(&o, "BEGIN:VCARD"); emit(&o, "VERSION:3.0");
    escaped(line, sizeof line, "FN:", c->name, ""); emit(&o, line);
    escaped(line, sizeof line, "N:", c->name, ";;;;"); emit(&o, line);
    for (int i = 0; i < c->count && i < INK_VC_NUMBERS; i++) {
        snprintf(line, sizeof line, "TEL;TYPE=%s%s:%.24s", TYPES[c->type[i] & 3], i == c->def ? ",PREF" : "", c->num[i]); emit(&o, line);
    }
    if (c->note[0]) { escaped(line, sizeof line, "NOTE:", c->note, ""); emit(&o, line); }
    emit(&o, "END:VCARD");
    return o.over ? -1 : (int)o.len;
}
