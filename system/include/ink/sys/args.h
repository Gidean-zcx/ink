/*
 * Capability arguments (D-55): a small fixed-size table of typed values. A
 * capability called on this device gets its arguments as C values, never as
 * JSON text (D-46); only calls that cross the boundary (the AI, the hub) are
 * converted from and to JSON, once, and checked against the capability's
 * JSON Schema there.
 *
 * Values: booleans, integers, strings (copied into the table), nested JSON
 * (an object or array kept as text, for the few capabilities that need
 * structure) and blobs (a pointer to the caller's buffer, e.g. a photo in
 * PSRAM: never copied, valid until the call completes).
 */
#ifndef INK_SYS_ARGS_H
#define INK_SYS_ARGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INK_ARGS_MAX 8     /* values per call (the largest capability has 2 today) */
#define INK_ARGS_TEXT 416  /* keys, a 354-byte SMS, sender and event member */

typedef enum { INK_ARG_NONE = 0, INK_ARG_BOOL, INK_ARG_INT, INK_ARG_STR, INK_ARG_JSON, INK_ARG_BLOB } ink_arg_type_t;

typedef struct {
    uint8_t type;      /* ink_arg_type_t */
    uint16_t key;      /* offset of the NUL-terminated key in text[] */
    uint16_t off, len; /* string / JSON value: offset and length in text[] (NUL-terminated) */
    int64_t i;         /* boolean (0/1) or integer value */
    const void *blob;  /* INK_ARG_BLOB: the caller's buffer, not copied */
    size_t blob_len;
} ink_arg_t;

typedef struct {
    uint8_t n;
    uint16_t used; /* bytes of text[] in use */
    ink_arg_t a[INK_ARGS_MAX];
    char text[INK_ARGS_TEXT];
} ink_args_t;

void ink_args_init(ink_args_t *a);
/* Add a value (a key given twice keeps the first). 0, or -1 when the table or its text is full. */
int ink_args_bool(ink_args_t *a, const char *key, bool v);
int ink_args_int(ink_args_t *a, const char *key, int64_t v);
int ink_args_str(ink_args_t *a, const char *key, const char *s);
int ink_args_json(ink_args_t *a, const char *key, const char *json, size_t len);
int ink_args_blob(ink_args_t *a, const char *key, const void *p, size_t n);

const ink_arg_t *ink_args_find(const ink_args_t *a, const char *key);
static inline const char *ink_args_key(const ink_args_t *a, const ink_arg_t *v) { return a->text + v->key; }
bool ink_args_get_bool(const ink_args_t *a, const char *key, bool *out);
bool ink_args_get_int(const ink_args_t *a, const char *key, int64_t *out);
/** String (or nested JSON text) value: its length, copied NUL-terminated (truncated to cap); -1 if absent. */
int ink_args_get_str(const ink_args_t *a, const char *key, char *out, size_t cap);

/**
 * Check against a JSON Schema (object). The subset the capabilities use: per property `type`
 * (integer, boolean, string, object, array), `minimum`, `maximum`, `enum`, `maxLength`
 * (characters); `required`; `additionalProperties: false`. Other keywords are ignored.
 * `schema` NULL accepts anything. 0, or -1 with the reason in `why`.
 */
int ink_args_check(const ink_args_t *a, const char *schema, char *why, size_t why_cap);
/** A JSON object (the AI's or the hub's arguments) into a table, then ink_args_check. 0 or -1 (why). */
int ink_args_from_json(ink_args_t *a, const char *json, size_t len, const char *schema, char *why, size_t why_cap);
/** The table as a JSON object (blobs as their length only); bytes written or -1. */
int ink_args_to_json(const ink_args_t *a, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
