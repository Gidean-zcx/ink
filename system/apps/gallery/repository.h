/* The gallery's pure helpers, copied from AgentUI (vendor/agentui) behind a C interface. Any task. */
#ifndef INK_APP_GALLERY_REPOSITORY_H
#define INK_APP_GALLERY_REPOSITORY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* One photo of the list, in PSRAM. modified: UTC seconds of the file time, -1 unknown. */
typedef struct {
    uint32_t id;
    int64_t modified;
    const char *name; /* the file name, inside the caller's path pool */
    uint16_t album;
    char month[8];    /* "YYYY-MM", or "undated" */
    int64_t size;
    const char *path; /* the whole path, in the same pool */
} ink_gallery_item_t;

/* Newest first, equal times by name descending (AgentUI's comparator); n <= 1024. */
void ink_gallery_sort(ink_gallery_item_t *items, size_t n);
void ink_gallery_day_label(int64_t utc_s, char *out, size_t cap);              /* "11月13日" */
void ink_gallery_date_key(int64_t utc_s, char sep, char *out, size_t cap);     /* "2026-11-13" */
void ink_gallery_time_label(int64_t utc_s, char *out, size_t cap);             /* "14:05" */
void ink_gallery_size_label(int64_t bytes, char *out, size_t cap);             /* "123 KB" */
bool ink_gallery_is_jpg(const char *name);
/* cb once per run of neighbours with the same month, in order: [begin, end). */
void ink_gallery_month_groups(const ink_gallery_item_t *items, size_t n,
                              void (*cb)(void *ctx, size_t begin, size_t end), void *ctx);

#ifdef __cplusplus
}
#endif
#endif
