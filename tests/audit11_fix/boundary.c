/* Test boundary for B11-01 and B11-02: one injected status error and index state probes. */
#include "ink/osal.h"
#include "ink/sys/data.h"
#include "ink/hal_storage.h"

static bool fault_armed;
static unsigned faults;
static int64_t checked_size(const char *path);
static int checked_exists(const char *path) __attribute__((unused)); /* unused when the fix is reverted */
#define hal_fs_size checked_size
#define hal_fs_exists checked_exists
#include "../../system/services/file/file_service.c"
#undef hal_fs_size
#undef hal_fs_exists

/* Fail once, only while pruning: every root is scanned and the file still exists. */
static bool fail_once(const char *path) {
    if (!fault_armed || !storage_index_scanned() || strcmp(path, "/content/SYNTH_0010.txt")) return false;
    fault_armed = false; faults++;
    return true;
}
static int64_t checked_size(const char *path) { return fail_once(path) ? -1 : hal_fs_size(path); }
static int checked_exists(const char *path) { return fail_once(path) ? -1 : hal_fs_exists(path); }

void ink_test_audit11_arm(void) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    fault_armed = true;
    osal_mutex_unlock(FS.edits);
}
unsigned ink_test_audit11_faults(void) { return faults; }
bool ink_test_audit11_idle(void) { return !FS.index_at.armed && !FS.index_job.queued; }
bool ink_test_audit11_between_batches(void) {
    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);
    bool between = storage_index_scanned() && storage.saving && storage.prune.started && !index_walk &&
        FS.index_at.armed && !FS.index_job.queued && ink_db_idle() && !ink_storage_pending();
    osal_mutex_unlock(FS.edits);
    return between;
}
unsigned ink_test_audit11_count(void) {
    ink_db_t *db = ink_db_open(&INDEX_DEF, NULL);
    unsigned n = db ? (unsigned)ink_db_count(db) : 0;
    ink_db_close(db); return n;
}
unsigned ink_test_audit11_id(const char *path) {
    ink_db_t *db = ink_db_open(&INDEX_DEF, NULL);
    index_rec_t e; ink_rec_t rec; ink_db_cursor_t cur = {0}; unsigned id = 0;
    while (db && index_next(db, &cur, &rec, &e) == 1)
        if (!memcmp(e.card, FS.index_card, 16) && !strcmp(e.path, path)) { id = rec.id; break; }
    ink_db_close(db); return id;
}
bool ink_test_audit11_done(void) {
    ink_db_t *db = ink_db_open(&INDEX_DONE_DEF, NULL);
    index_rec_t e; ink_rec_t rec;
    bool done = index_saved(db, FS.index_card, &e, &rec) != 0;
    ink_db_close(db); return done;
}
