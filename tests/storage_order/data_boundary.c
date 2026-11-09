#include <string.h>
#include "ink/sys/storage.h"
#include "ink/sys/data.h"
static unsigned writes;
#ifdef INK_ORDER_BASELINE
static int counted_append(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx);
static int counted_snapshot(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx);
#else
static int counted_append(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx, const unsigned *waiters);
static int counted_snapshot(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx, const unsigned *waiters);
#endif
#define ink_storage_append counted_append
#define ink_storage_write_borrow counted_snapshot
#ifndef INK_DATA_SOURCE
#define INK_DATA_SOURCE "../../system/services/data/datastore.c"
#endif
#include INK_DATA_SOURCE
#undef ink_storage_append
#undef ink_storage_write_borrow
#ifdef INK_ORDER_BASELINE
void ink_db_flush_background(ink_db_t *db) { ink_db_flush(db); }
#endif
#ifdef INK_ORDER_BASELINE
static int counted_append(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {
    const unsigned *waiters = NULL;
#else
static int counted_append(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx, const unsigned *waiters) {
#endif
    int r = ink_storage_append(path, data, len, done, ctx, waiters);
    if (!r && strstr(path, "/file.index")) writes++;
    if (!r && strstr(path, "/file.index_ready."))
        for (int i = 0; i < MAX_COLLECTIONS; i++)
            if (DB.c[i].used && !strcmp(DB.c[i].def->name, "file.index") && DB.c[i].durable_ver != DB.c[i].last_ver)
                ink_probe_count("test.fail");
    return r;
}
#ifdef INK_ORDER_BASELINE
static int counted_snapshot(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {
    const unsigned *waiters = NULL;
#else
static int counted_snapshot(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx, const unsigned *waiters) {
#endif
    int r = ink_storage_write_borrow(path, data, len, done, ctx, waiters);
    if (!r && strstr(path, "/file.index")) writes++;
    return r;
}
unsigned ink_test_index_writes(void) { return writes; }
size_t ink_test_pending_bytes(ink_db_t *db) { lock(); size_t n = db->pend_len; unlock(); return n; }
size_t ink_test_pending_limit(void) { return PEND_MAX; }
