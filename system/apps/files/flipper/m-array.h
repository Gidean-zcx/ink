/* Shim: M*LIB's ARRAY_DEF as a plain view of the file pages' items (only get and size are used). */
#pragma once
#include <stddef.h>
#define ARRAY_DEF(name, type, ...) typedef struct { type *items; size_t count; } name##_s, name##_t[1]; \
    static inline type *name##_get(name##_t a, size_t i) { return i < a->count ? &a->items[i] : NULL; }
