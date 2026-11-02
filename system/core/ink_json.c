/* JSON helpers over jsmn (no heap). */
#include <stdio.h>
#include <string.h>

#define JSMN_STATIC
#define JSMN_PARENT_LINKS
#include "jsmn.h"
#include "ink/osal.h"
#include "ink/sys/json.h"

int ink_json_parse(ink_json_t *j, const char *text, size_t len) {
    jsmn_parser p;
    jsmntok_t toks[INK_JSON_MAX_TOKENS];
    jsmn_init(&p);
    int n = jsmn_parse(&p, text, len, toks, INK_JSON_MAX_TOKENS);
    osal_cost_units("json.byte", len);
    if (n < 1) return -1;
    j->text = text;
    j->len = len;
    j->ntok = n;
    for (int i = 0; i < n; i++) {
        j->tok[i].type = toks[i].type;
        j->tok[i].start = toks[i].start;
        j->tok[i].end = toks[i].end;
        j->tok[i].size = toks[i].size;
    }
    return 0;
}

/* index just past token t's whole subtree */
static int skip(const ink_json_t *j, int t) {
    int end = j->tok[t].end, i = t + 1;
    while (i < j->ntok && j->tok[i].start < end) i++;
    return i;
}

bool ink_json_eq(const ink_json_t *j, int t, const char *s) {
    if (t < 0 || t >= j->ntok) return false;
    size_t n = (size_t)(j->tok[t].end - j->tok[t].start);
    return strlen(s) == n && !memcmp(j->text + j->tok[t].start, s, n);
}

static int find_key(const ink_json_t *j, int obj, const char *key, size_t klen) {
    if (obj < 0 || obj >= j->ntok || j->tok[obj].type != JSMN_OBJECT) return -1;
    int i = obj + 1, end = j->tok[obj].end;
    while (i < j->ntok && j->tok[i].start < end) {
        size_t n = (size_t)(j->tok[i].end - j->tok[i].start);
        int val = i + 1;
        if (j->tok[i].type == JSMN_STRING && n == klen && !memcmp(j->text + j->tok[i].start, key, klen))
            return val < j->ntok ? val : -1;
        i = val < j->ntok ? skip(j, val) : j->ntok;
    }
    return -1;
}

int ink_json_find(const ink_json_t *j, int obj, const char *path) {
    int cur = obj;
    while (*path) {
        const char *dot = strchr(path, '.');
        size_t n = dot ? (size_t)(dot - path) : strlen(path);
        cur = find_key(j, cur, path, n);
        if (cur < 0) return -1;
        path += n + (dot ? 1 : 0);
    }
    return cur;
}

static int hexv(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

int ink_json_str(const ink_json_t *j, int t, char *out, size_t cap) {
    if (t < 0 || t >= j->ntok || j->tok[t].type != JSMN_STRING || !cap) return -1;
    const char *s = j->text + j->tok[t].start, *e = j->text + j->tok[t].end;
    size_t o = 0;
    while (s < e && o + 1 < cap) {
        char c = *s++;
        if (c != '\\' || s >= e) {
            out[o++] = c;
            continue;
        }
        c = *s++;
        switch (c) {
        case 'n': out[o++] = '\n'; break;
        case 't': out[o++] = '\t'; break;
        case 'r': out[o++] = '\r'; break;
        case 'b': out[o++] = '\b'; break;
        case 'f': out[o++] = '\f'; break;
        case 'u': {
            if (e - s < 4) return -1;
            int cp = 0;
            for (int k = 0; k < 4; k++) {
                int h = hexv(s[k]);
                if (h < 0) return -1;
                cp = cp * 16 + h;
            }
            s += 4;
            if (cp >= 0xD800 && cp <= 0xDBFF && e - s >= 6 && s[0] == '\\' && s[1] == 'u') { /* surrogate pair */
                int lo = 0;
                for (int k = 0; k < 4; k++) lo = lo * 16 + hexv(s[2 + k]);
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                s += 6;
            }
            char tmp[4];
            int n = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
            if (n == 1) tmp[0] = (char)cp;
            else if (n == 2) { tmp[0] = (char)(0xC0 | cp >> 6); tmp[1] = (char)(0x80 | (cp & 0x3F)); }
            else if (n == 3) { tmp[0] = (char)(0xE0 | cp >> 12); tmp[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); tmp[2] = (char)(0x80 | (cp & 0x3F)); }
            else { tmp[0] = (char)(0xF0 | cp >> 18); tmp[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); tmp[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); tmp[3] = (char)(0x80 | (cp & 0x3F)); }
            if (o + (size_t)n >= cap) return -1;
            memcpy(out + o, tmp, (size_t)n);
            o += (size_t)n;
            break;
        }
        default: out[o++] = c; break; /* \" \\ \/ */
        }
    }
    out[o] = 0;
    return (int)o;
}

bool ink_json_int(const ink_json_t *j, int t, int64_t *out) {
    if (t < 0 || t >= j->ntok || j->tok[t].type != JSMN_PRIMITIVE) return false;
    char tmp[24];
    int n = j->tok[t].end - j->tok[t].start;
    if (n <= 0 || n >= (int)sizeof tmp) return false;
    memcpy(tmp, j->text + j->tok[t].start, (size_t)n);
    tmp[n] = 0;
    long long v;
    if (sscanf(tmp, "%lld", &v) != 1) return false;
    *out = (int64_t)v;
    return true;
}

void ink_jw_init(ink_jw_t *w, char *buf, size_t cap) {
    w->buf = buf;
    w->cap = cap;
    w->len = 0;
    w->overflow = false;
    if (cap) buf[0] = 0;
}

static void put(ink_jw_t *w, const char *s, size_t n) {
    if (w->len + n + 1 > w->cap) {
        w->overflow = true;
        return;
    }
    memcpy(w->buf + w->len, s, n);
    w->len += n;
    w->buf[w->len] = 0;
}

void ink_jw_raw(ink_jw_t *w, const char *s) { put(w, s, strlen(s)); }

void ink_jw_strn(ink_jw_t *w, const char *s, size_t n) {
    put(w, "\"", 1);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        char esc[8];
        if (c == '"' || c == '\\') {
            esc[0] = '\\';
            esc[1] = (char)c;
            put(w, esc, 2);
        } else if (c == '\n') put(w, "\\n", 2);
        else if (c == '\r') put(w, "\\r", 2);
        else if (c == '\t') put(w, "\\t", 2);
        else if (c < 0x20) {
            snprintf(esc, sizeof esc, "\\u%04x", c);
            put(w, esc, 6);
        } else put(w, (const char *)&s[i], 1);
    }
    put(w, "\"", 1);
}

void ink_jw_str(ink_jw_t *w, const char *s) { ink_jw_strn(w, s, strlen(s)); }

void ink_jw_int(ink_jw_t *w, int64_t v) {
    char tmp[24];
    snprintf(tmp, sizeof tmp, "%lld", (long long)v);
    put(w, tmp, strlen(tmp));
}
