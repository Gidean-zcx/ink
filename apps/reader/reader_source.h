#ifndef INK_READER_SOURCE_H
#define INK_READER_SOURCE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ink_reader_source ink_reader_source_t;
typedef enum { INK_READER_OK, INK_READER_IO, INK_READER_CORRUPT, INK_READER_ENCRYPTED,
    INK_READER_FORMAT, INK_READER_MEMORY, INK_READER_CANCELLED } ink_reader_error_t;
typedef struct { char title[128], author[96]; int chapters, entries; bool epub, fallback; } ink_reader_info_t;
typedef struct { char title[128]; int chapter; uint64_t offset; } ink_reader_entry_t;
typedef bool (*ink_reader_cancel_fn)(void *ctx);
ink_reader_error_t ink_reader_source_open(const char *path, ink_reader_source_t **out, ink_reader_info_t *info);
int ink_reader_source_toc(ink_reader_source_t *s, int index, ink_reader_entry_t *entry);
ink_reader_error_t ink_reader_source_read(ink_reader_source_t *s, int chapter, uint64_t offset,
    char *out, size_t cap, size_t *length, bool *more, ink_reader_cancel_fn cancel, void *ctx);
void ink_reader_source_close(ink_reader_source_t *s);
#ifdef __cplusplus
}
#endif
#endif
