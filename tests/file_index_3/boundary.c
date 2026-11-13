#include "ink/osal.h"
#include "ink/sys/data.h"
#include "ink/hal_storage.h"
#include "ink/sys/exec.h"
static unsigned directory_reads, comparisons, batches;
static unsigned marker_writes;
static unsigned size_queries;
static osal_task_t *edit_owner;
static uint64_t hold_start, max_hold;
static int counted_next(ink_db_t *db, ink_db_cursor_t *c, ink_rec_t *out, void *buf, size_t cap);
static osal_err_t counted_lock(osal_mutex_t *m, int64_t timeout);
static void counted_unlock(osal_mutex_t *m);
static int counted_readdir(hal_dir_t *d, hal_dirent_t *e);
static hal_file_t *checked_open(const char *path, int flags);
static int64_t counted_size(const char *path);
static bool fail_identity_open;
static int counted_post(ink_exec_t e, ink_exec_job_t *job, void (*fn)(void *), void *arg);
#define ink_db_next counted_next
#define osal_mutex_lock counted_lock
#define osal_mutex_unlock counted_unlock
#define hal_fs_readdir counted_readdir
#define hal_fs_open checked_open
#define hal_fs_size counted_size
#define ink_exec_post counted_post
#ifndef INK_INDEX_SOURCE
#define INK_INDEX_SOURCE "../../system/services/file/file_service.c"
#endif
#include INK_INDEX_SOURCE
#undef ink_db_next
#undef osal_mutex_lock
#undef osal_mutex_unlock
#undef hal_fs_readdir
#undef hal_fs_open
#undef hal_fs_size
#undef ink_exec_post
static int64_t counted_size(const char *path) { size_queries++; return hal_fs_size(path); }
static hal_file_t *checked_open(const char *path, int flags) {
    if (fail_identity_open && flags == HAL_O_READ && !strcmp(path, INDEX_CARD)) {
        fail_identity_open = false;
        return NULL;
    }
    hal_file_t *f = hal_fs_open(path, flags);
    if (f && (flags & HAL_O_CREATE) && !strcmp(path, "/content/.file-index-dirty")) marker_writes++;
    return f;
}
unsigned ink_test_index_marker_writes(void) { return marker_writes; }
/* The executor clears `queued` as the index job starts: idle must also wait for a run in progress, or a test
 * starts measuring halfway through it (and its own index reads then stretch that run's edits hold). */
static void (*index_fn)(void *);
static int index_running;
static void counted_index(void *arg) {
    __atomic_add_fetch(&index_running, 1, __ATOMIC_ACQ_REL);
    index_fn(arg);
    __atomic_sub_fetch(&index_running, 1, __ATOMIC_ACQ_REL);
}
static int counted_post(ink_exec_t e, ink_exec_job_t *job, void (*fn)(void *), void *arg) {
    if (job == &FS.index_job) { index_fn = fn; fn = counted_index; }
    return ink_exec_post(e, job, fn, arg);
}
bool ink_test_index_idle(void) {
    return !FS.index_at.armed && !FS.index_job.queued && !__atomic_load_n(&index_running, __ATOMIC_ACQUIRE);
}
unsigned ink_test_index_load_size_queries(void) {
    index_walk_t *w = osal_calloc(1, sizeof *w, OSAL_MEM_EXTERNAL);
    ink_db_t *db = ink_db_open(&INDEX_DEF, NULL);
    unsigned before = size_queries;
    int r = w && db ? index_walk_load(w, db, FS.index_card) : -1;
    unsigned queries = size_queries - before;
    ink_db_close(db); osal_free(w);
    return r ? (unsigned)-1 : queries;
}
void ink_test_fail_identity_open(void) { fail_identity_open = true; }
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
    uint8_t raw[216]; ink_rec_t rec; ink_db_cursor_t cur = {0}; unsigned id = 0;
    while (db && ink_db_next(db, &cur, &rec, raw, sizeof raw) == 1)
        if (!memcmp(raw, FS.index_card, 16) && !strcmp((const char *)raw + (rec.schema == 3 ? 32 : 16), path)) { id = rec.id; break; }
    ink_db_close(db); return id;
}
bool ink_test_index_locked(void) { return edit_owner == osal_task_self(); }
void ink_test_index_report(void) {
    ink_probe_value("test.index.max_hold_us", (double)max_hold);
    ink_probe_value("test.index.edit_sections", batches);
    ink_probe_value("test.index.comparisons", comparisons);
    ink_probe_value("test.index.dir_reads", directory_reads);
}
