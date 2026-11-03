/*
 * Capability arguments (see ink/sys/args.h): the typed table, its JSON
 * boundary and the JSON Schema subset the capabilities use.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/sys/args.h"
#include "ink/sys/json.h"

enum { JT_OBJECT = 1, JT_ARRAY = 2, JT_STRING = 4, JT_PRIMITIVE = 8 }; /* jsmn token types */

void ink_args_init(ink_args_t *a) {
    a->n = 0;
    a->used = 0;
    a->text[0] = 0;
}

/* Copy n bytes (+ NUL) into the text area: the offset, or -1 when it does not fit. */
static int put_text(ink_args_t *a, const char *s, size_t n) {
    if ((size_t)a->used + n + 1 > INK_ARGS_TEXT) return -1;
    int off = a->used;
    memcpy(a->text + off, s, n);
    a->text[off + n] = 0;
    a->used = (uint16_t)(a->used + n + 1);
    return off;
}

const ink_arg_t *ink_args_find(const ink_args_t *a, const char *key) {
    for (int i = 0; i < a->n; i++)
        if (!strcmp(a->text + a->a[i].key, key)) return &a->a[i];
    return NULL;
}

/* A new slot with its key, or NULL: *dup is set when the key is already there (kept as is). */
static ink_arg_t *add(ink_args_t *a, const char *key, ink_arg_type_t type, bool *dup) {
    *dup = ink_args_find(a, key) != NULL;
    if (*dup || a->n >= INK_ARGS_MAX) return NULL;
    int k = put_text(a, key, strlen(key));
    if (k < 0) return NULL;
    ink_arg_t *v = &a->a[a->n];
    memset(v, 0, sizeof *v);
    v->type = (uint8_t)type;
    v->key = (uint16_t)k;
    return v;
}

static int add_scalar(ink_args_t *a, const char *key, ink_arg_type_t type, int64_t i) {
    bool dup;
    ink_arg_t *v = add(a, key, type, &dup);
    if (!v) return dup ? 0 : -1;
    v->i = i;
    a->n++;
    return 0;
}

int ink_args_bool(ink_args_t *a, const char *key, bool v) { return add_scalar(a, key, INK_ARG_BOOL, v ? 1 : 0); }
int ink_args_int(ink_args_t *a, const char *key, int64_t v) { return add_scalar(a, key, INK_ARG_INT, v); }

static int add_text(ink_args_t *a, const char *key, ink_arg_type_t type, const char *s, size_t n) {
    uint16_t used = a->used;
    bool dup;
    ink_arg_t *v = add(a, key, type, &dup);
    if (!v) return dup ? 0 : -1;
    int off = put_text(a, s, n);
    if (off < 0) {
        a->used = used; /* drop the key too */
        return -1;
    }
    v->off = (uint16_t)off;
    v->len = (uint16_t)n;
    a->n++;
    return 0;
}

int ink_args_str(ink_args_t *a, const char *key, const char *s) { return add_text(a, key, INK_ARG_STR, s, strlen(s)); }
int ink_args_json(ink_args_t *a, const char *key, const char *json, size_t len) { return add_text(a, key, INK_ARG_JSON, json, len); }

int ink_args_blob(ink_args_t *a, const char *key, const void *p, size_t n) {
    bool dup;
    ink_arg_t *v = add(a, key, INK_ARG_BLOB, &dup);
    if (!v) return dup ? 0 : -1;
    v->blob = p;
    v->blob_len = n;
    a->n++;
    return 0;
}

bool ink_args_get_bool(const ink_args_t *a, const char *key, bool *out) {
    const ink_arg_t *v = ink_args_find(a, key);
    if (!v || v->type != INK_ARG_BOOL) return false;
    *out = v->i != 0;
    return true;
}

bool ink_args_get_int(const ink_args_t *a, const char *key, int64_t *out) {
    const ink_arg_t *v = ink_args_find(a, key);
    if (!v || v->type != INK_ARG_INT) return false;
    *out = v->i;
    return true;
}

int ink_args_get_str(const ink_args_t *a, const char *key, char *out, size_t cap) {
    const ink_arg_t *v = ink_args_find(a, key);
    if (!v || (v->type != INK_ARG_STR && v->type != INK_ARG_JSON) || !cap) return -1;
    size_t n = v->len < cap - 1 ? v->len : cap - 1;
    memcpy(out, a->text + v->off, n);
    out[n] = 0;
    return v->len;
}

/* ------------------------------------------------------------ schema subset */

static int skip(const ink_json_t *j, int t) {
    int end = j->tok[t].end, i = t + 1;
    while (i < j->ntok && j->tok[i].start < end) i++;
    return i;
}

/* Value token of `key` in object token `obj` (keys are compared whole: they may contain dots). */
static int member(const ink_json_t *j, int obj, const char *key) {
    if (obj < 0 || j->tok[obj].type != JT_OBJECT) return -1;
    size_t n = strlen(key);
    int i = obj + 1;
    for (int k = 0; k < j->tok[obj].size && i + 1 < j->ntok; k++) {
        const ink_jtok_t *t = &j->tok[i];
        if (t->type == JT_STRING && (size_t)(t->end - t->start) == n && !memcmp(j->text + t->start, key, n)) return i + 1;
        i = skip(j, i + 1);
    }
    return -1;
}

static size_t utf8_chars(const char *s, size_t n) {
    size_t c = 0;
    for (size_t i = 0; i < n; i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80) c++;
    return c;
}

static bool in_enum(const ink_json_t *s, int en, const ink_args_t *a, const ink_arg_t *v) {
    int i = en + 1;
    for (int k = 0; k < s->tok[en].size && i < s->ntok; k++) {
        if (v->type == INK_ARG_STR && s->tok[i].type == JT_STRING && (size_t)(s->tok[i].end - s->tok[i].start) == v->len &&
            !memcmp(s->text + s->tok[i].start, a->text + v->off, v->len))
            return true;
        int64_t x;
        if (v->type == INK_ARG_INT && ink_json_int(s, i, &x) && x == v->i) return true;
        i = skip(s, i);
    }
    return false;
}

/* One value against its property schema `p`. */
static int check_value(const ink_json_t *s, int p, const ink_args_t *a, const ink_arg_t *v, char *why, size_t cap) {
    const char *key = a->text + v->key;
    int t = member(s, p, "type");
    if (t >= 0) {
        bool ok;
        if (ink_json_eq(s, t, "integer") || ink_json_eq(s, t, "number")) ok = v->type == INK_ARG_INT;
        else if (ink_json_eq(s, t, "boolean")) ok = v->type == INK_ARG_BOOL;
        else if (ink_json_eq(s, t, "string")) ok = v->type == INK_ARG_STR;
        else if (ink_json_eq(s, t, "object")) ok = v->type == INK_ARG_JSON && a->text[v->off] == '{';
        else if (ink_json_eq(s, t, "array")) ok = v->type == INK_ARG_JSON && a->text[v->off] == '[';
        else ok = true;
        if (!ok) {
            snprintf(why, cap, "%s has the wrong type", key);
            return -1;
        }
    }
    int64_t lim;
    if (v->type == INK_ARG_INT && ink_json_int(s, member(s, p, "minimum"), &lim) && v->i < lim) {
        snprintf(why, cap, "%s must be >= %lld", key, (long long)lim);
        return -1;
    }
    if (v->type == INK_ARG_INT && ink_json_int(s, member(s, p, "maximum"), &lim) && v->i > lim) {
        snprintf(why, cap, "%s must be <= %lld", key, (long long)lim);
        return -1;
    }
    if (v->type == INK_ARG_STR && ink_json_int(s, member(s, p, "maxLength"), &lim) && (int64_t)utf8_chars(a->text + v->off, v->len) > lim) {
        snprintf(why, cap, "%s is longer than %lld characters", key, (long long)lim);
        return -1;
    }
    int en = member(s, p, "enum");
    if (en >= 0 && s->tok[en].type == JT_ARRAY && !in_enum(s, en, a, v)) {
        snprintf(why, cap, "%s is not one of the allowed values", key);
        return -1;
    }
    return 0;
}

int ink_args_check(const ink_args_t *a, const char *schema, char *why, size_t why_cap) {
    if (!schema) return 0;
    ink_json_t *s = osal_malloc(sizeof *s, OSAL_MEM_EXTERNAL);
    if (!s) {
        snprintf(why, why_cap, "out of memory");
        return -1;
    }
    int r = -1;
    if (ink_json_parse(s, schema, strlen(schema)) != 0 || s->tok[0].type != JT_OBJECT) {
        snprintf(why, why_cap, "the capability's schema is not a JSON object");
        goto out;
    }
    int props = member(s, 0, "properties");
    int extra = member(s, 0, "additionalProperties");
    bool closed = extra >= 0 && ink_json_eq(s, extra, "false");
    for (int i = 0; i < a->n; i++) {
        const ink_arg_t *v = &a->a[i];
        int p = member(s, props, a->text + v->key);
        if (p < 0) {
            if (closed) {
                snprintf(why, why_cap, "unexpected argument %s", a->text + v->key);
                goto out;
            }
            continue;
        }
        if (check_value(s, p, a, v, why, why_cap)) goto out;
    }
    int req = member(s, 0, "required");
    if (req >= 0 && s->tok[req].type == JT_ARRAY) {
        int i = req + 1;
        for (int k = 0; k < s->tok[req].size && i < s->ntok; k++) {
            char key[48];
            if (ink_json_str(s, i, key, sizeof key) >= 0 && !ink_args_find(a, key)) {
                snprintf(why, why_cap, "%s required", key);
                goto out;
            }
            i = skip(s, i);
        }
    }
    r = 0;
out:
    osal_free(s);
    return r;
}

int ink_args_from_json(ink_args_t *a, const char *json, size_t len, const char *schema, char *why, size_t why_cap) {
    ink_args_init(a);
    if (!json || !len) return ink_args_check(a, schema, why, why_cap);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (!j) {
        snprintf(why, why_cap, "out of memory");
        return -1;
    }
    int r = -1;
    if (ink_json_parse(j, json, len) != 0 || j->tok[0].type != JT_OBJECT) {
        snprintf(why, why_cap, "arguments are not a JSON object");
        goto out;
    }
    int i = 1;
    for (int k = 0; k < j->tok[0].size && i + 1 < j->ntok; k++) {
        char key[48];
        int vt = i + 1;
        const ink_jtok_t *v = &j->tok[vt];
        if (ink_json_str(j, i, key, sizeof key) < 0 || (size_t)(j->tok[i].end - j->tok[i].start) >= sizeof key) {
            snprintf(why, why_cap, "argument name too long");
            goto out;
        }
        const char *raw = json + v->start;
        size_t rawn = (size_t)(v->end - v->start);
        int put = 0;
        if (v->type == JT_STRING) {
            char *tmp = osal_malloc(rawn + 1, OSAL_MEM_EXTERNAL);
            if (!tmp) {
                snprintf(why, why_cap, "out of memory");
                goto out;
            }
            int n = ink_json_str(j, vt, tmp, rawn + 1);
            put = n < 0 ? -1 : add_text(a, key, INK_ARG_STR, tmp, (size_t)n);
            osal_free(tmp);
        } else if (v->type == JT_PRIMITIVE) {
            int64_t x;
            if (raw[0] == 't' || raw[0] == 'f') put = ink_args_bool(a, key, raw[0] == 't');
            else if (raw[0] == 'n') put = 0; /* null: as if absent */
            else if (memchr(raw, '.', rawn) || memchr(raw, 'e', rawn) || memchr(raw, 'E', rawn) || !ink_json_int(j, vt, &x)) {
                snprintf(why, why_cap, "%s must be an integer", key);
                goto out;
            } else put = ink_args_int(a, key, x);
        } else put = add_text(a, key, INK_ARG_JSON, raw, rawn);
        if (put) {
            snprintf(why, why_cap, "arguments too long");
            goto out;
        }
        i = skip(j, vt);
    }
    r = ink_args_check(a, schema, why, why_cap);
out:
    osal_free(j);
    return r;
}

int ink_args_to_json(const ink_args_t *a, char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "{");
    for (int i = 0; i < a->n; i++) {
        const ink_arg_t *v = &a->a[i];
        if (i) ink_jw_raw(&w, ",");
        ink_jw_str(&w, a->text + v->key);
        ink_jw_raw(&w, ":");
        switch (v->type) {
        case INK_ARG_BOOL: ink_jw_raw(&w, v->i ? "true" : "false"); break;
        case INK_ARG_INT: ink_jw_int(&w, v->i); break;
        case INK_ARG_STR: ink_jw_strn(&w, a->text + v->off, v->len); break;
        case INK_ARG_JSON: ink_jw_raw(&w, a->text + v->off); break;
        default:
            ink_jw_raw(&w, "{\"bytes\":");
            ink_jw_int(&w, (int64_t)v->blob_len);
            ink_jw_raw(&w, "}");
            break;
        }
    }
    ink_jw_raw(&w, "}");
    return w.overflow ? -1 : (int)w.len;
}
