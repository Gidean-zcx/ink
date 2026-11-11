#ifndef INK_READER_BOOKMARKS_H
#define INK_READER_BOOKMARKS_H
#include <reader_source.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { INK_BOOKMARK_LIMIT = 256, INK_BOOKMARK_EXCERPT = 768 };
typedef struct { uint32_t chapter, block, offset; } ink_reader_position_t;
typedef struct {
    uint32_t id, page;
    ink_reader_position_t start, end;
    int64_t added_at;
    char excerpt[INK_BOOKMARK_EXCERPT+1];
} ink_bookmark_t;
bool ink_reader_locate(ink_reader_source_t *s, ink_reader_position_t *position, uint64_t *flat, bool to_source);
bool ink_bookmarks_open(const char *member, const char *book);
void ink_bookmarks_close(void);
int ink_bookmarks_count(void);
bool ink_bookmarks_get(int index, ink_bookmark_t *out);
bool ink_bookmarks_add(const ink_bookmark_t *entry);
bool ink_bookmarks_remove(uint32_t id);
uint32_t ink_bookmarks_in_page(ink_reader_position_t first, ink_reader_position_t end);
#ifdef __cplusplus
}
#endif
#endif
