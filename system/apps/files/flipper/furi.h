/* Shim for the copied Flipper archive code (vendor/flipper): only what it calls. */
#pragma once
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define furi_assert(x) ((void)(x))
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))
#define UNUSED(x) ((void)(x))
#define _ATTRIBUTE(attrs) __attribute__(attrs)
#define FURI_STRING_FAILURE ((size_t)-1)
#define RECORD_STORAGE "storage"
#define RECORD_LOADER "loader"
typedef struct FuriString FuriString;
typedef struct View View;
typedef struct BrowserWorker BrowserWorker;
typedef struct FuriTimer FuriTimer;
typedef int InputKey;
FuriString *furi_string_alloc(void);
FuriString *furi_string_alloc_set_str(const char *s);
FuriString *furi_string_alloc_vprintf(const char *format, va_list args);
void furi_string_free(FuriString *s);
void furi_string_reset(FuriString *s);
void furi_string_set_str(FuriString *s, const char *v);
void furi_string_push_back(FuriString *s, char c);
size_t furi_string_size(const FuriString *s);
const char *furi_string_get_cstr(const FuriString *s);
size_t furi_string_search_str(const FuriString *s, const char *needle);
size_t furi_string_search_char(const FuriString *s, char c, size_t start);
void furi_string_right(FuriString *s, size_t start);
void furi_string_left(FuriString *s, size_t n);
int furi_string_printf(FuriString *s, const char *format, ...);
/* As Flipper's extension filters: "a|b" ends with either (case-insensitive). */
bool furi_string_end_withi(const FuriString *s, const char *ends);
#define FURI_STR_ARG(v) _Generic((v), FuriString *: furi_string_get_cstr((const FuriString *)(v)), default: (const char *)(v))
#define furi_string_set(s, v) furi_string_set_str((s), FURI_STR_ARG(v))
#define furi_string_alloc_set(v) furi_string_alloc_set_str(FURI_STR_ARG(v))
#define furi_string_search(s, v) furi_string_search_str((s), FURI_STR_ARG(v))
/* The storage record is the favourites store's lock (reentrant on one task). */
void *furi_record_open(const char *name);
void furi_record_close(const char *name);
