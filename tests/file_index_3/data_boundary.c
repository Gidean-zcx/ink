#include <string.h>
#include "ink/sys/storage.h"
static unsigned appends, snapshots;
static int counted_append(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx);
static int counted_snapshot(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx);
#define ink_storage_append counted_append
#define ink_storage_write_borrow counted_snapshot
#include "../../system/services/data/datastore.c"
#undef ink_storage_append
#undef ink_storage_write_borrow
static int counted_append(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {
    int r = ink_storage_append(path, data, len, done, ctx);
    if (!r && strstr(path, "/file.index")) appends++;
    return r;
}
static int counted_snapshot(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {
    int r = ink_storage_write_borrow(path, data, len, done, ctx);
    if (!r && strstr(path, "/file.index")) snapshots++;
    return r;
}
unsigned ink_test_index_writes(void) { return appends + snapshots; }
