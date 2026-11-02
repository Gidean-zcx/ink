/*
 * Small JSON helpers for hub messages, on top of the zero-allocation jsmn
 * tokenizer. Parsing works in place on the received text; writing appends to
 * a caller-owned buffer. No heap use at all.
 */
#ifndef INK_SYS_JSON_H
#define INK_SYS_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INK_JSON_MAX_TOKENS 128

typedef struct {
    int type;  /* 1 object, 2 array, 4 string, 8 primitive (jsmn values) */
    int start, end, size;
} ink_jtok_t;

typedef struct {
    const char *text;
    size_t len;
    ink_jtok_t tok[INK_JSON_MAX_TOKENS];
    int ntok;
} ink_json_t;

/** Tokenize; returns 0 or -1 (invalid / too many tokens). */
int ink_json_parse(ink_json_t *j, const char *text, size_t len);
/** Index of the value of `key` inside object token `obj` (0 = root), or -1. Dotted paths allowed: "params.id". */
int ink_json_find(const ink_json_t *j, int obj, const char *path);
bool ink_json_eq(const ink_json_t *j, int t, const char *s);
/** Copy a string value (JSON escapes decoded, \uXXXX -> UTF-8). Returns length or -1. */
int ink_json_str(const ink_json_t *j, int t, char *out, size_t cap);
bool ink_json_int(const ink_json_t *j, int t, int64_t *out);

/* ---- writer ---- */
typedef struct {
    char *buf;
    size_t cap, len;
    bool overflow;
} ink_jw_t;

void ink_jw_init(ink_jw_t *w, char *buf, size_t cap);
void ink_jw_raw(ink_jw_t *w, const char *s);
/** Append a JSON string literal with escaping (quotes included). */
void ink_jw_str(ink_jw_t *w, const char *s);
void ink_jw_strn(ink_jw_t *w, const char *s, size_t n);
void ink_jw_int(ink_jw_t *w, int64_t v);

#ifdef __cplusplus
}
#endif
#endif
