#include "ink/osal.h"
#include "ink/sys/data.h"
#include "ink/hal_storage.h"
static unsigned directory_reads, comparisons, batches;
static osal_task_t *edit_owner;
static uint64_t hold_start, max_hold;
static int counted_next(ink_db_t *db, ink_db_cursor_t *c, ink_rec_t *out, void *buf, size_t cap);
static osal_err_t counted_lock(osal_mutex_t *m, int64_t timeout);
static void counted_unlock(osal_mutex_t *m);
static int counted_readdir(hal_dir_t *d, hal_dirent_t *e);
#define ink_db_next counted_next
#define osal_mutex_lock counted_lock
#define osal_mutex_unlock counted_unlock
#define hal_fs_readdir counted_readdir
#ifndef INK_INDEX_SOURCE
#define INK_INDEX_SOURCE "../../system/services/file/file_service.c"
#endif
#include INK_INDEX_SOURCE
#undef ink_db_next
#undef osal_mutex_lock
#undef osal_mutex_unlock
#undef hal_fs_readdir
static int counted_readdir(hal_dir_t *d, hal_dirent_t *e) { directory_reads++; return hal_fs_readdir(d, e); }
static int counted_next(ink_db_t *db, ink_db_cursor_t *c, ink_rec_t *out, void *buf, size_t cap) {
    comparisons++;
    return ink_db_next(db, c, out, buf, cap);
}
static osal_err_t counted_lock(osal_mutex_t *m, int64_t timeout) {
    osal_err_t r = osal_mutex_lock(m, timeout);
    if (m == FS.edits && r == OSAL_OK) { edit_owner = osal_task_self(); hold_start = osal_now_us(); }
    return r;
}
static void counted_unlock(osal_mutex_t *m) {
    if (m == FS.edits) {
        uint64_t held = osal_now_us() - hold_start;
        if (held > 250000) ink_probe_value("test.index.long_hold_at_us", (double)osal_now_us());
        if (held > max_hold) max_hold = held;
        batches++; edit_owner = NULL;
    }
    osal_mutex_unlock(m);
}
unsigned ink_test_index_count(void) {
    ink_db_t *db = ink_db_open(&INDEX_DEF, NULL);
    unsigned n = db ? (unsigned)ink_db_count(db) : 0;
    ink_db_close(db); return n;
}
unsigned ink_test_index_comparisons(void) { return comparisons; }
unsigned ink_test_index_reads(void) { return directory_reads; }
unsigned ink_test_index_path_id(const char *path) {
    ink_db_t *db = ink_db_open(&INDEX_DEF, NULL);
    index_rec_t e; ink_rec_t rec; ink_db_cursor_t cur = {0}; unsigned id = 0;
    while (db && ink_db_next(db, &cur, &rec, &e, sizeof e) == 1)
        if (!memcmp(e.card, FS.index_card, 16) && !strcmp(e.path, path)) { id = rec.id; break; }
    ink_db_close(db); return id;
}
bool ink_test_index_locked(void) { return edit_owner == osal_task_self(); }
void ink_test_index_report(void) {
    ink_probe_value("test.index.max_hold_us", (double)max_hold);
    ink_probe_value("test.index.edit_sections", batches);
    ink_probe_value("test.index.comparisons", comparisons);
    ink_probe_value("test.index.dir_reads", directory_reads);
}
