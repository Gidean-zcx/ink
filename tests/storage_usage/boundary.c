#include "ink/hal_storage.h"
#include "ink/osal.h"
static unsigned reads, sizes, locked_sizes, edit_holds;
static uint64_t hold_start, max_hold_us, size_us;
static osal_task_t *edit_owner;
static bool measuring;
static void (*edit_on_size)(void);
static int counted_readdir(hal_dir_t *d, hal_dirent_t *e);
static int64_t counted_size(const char *path);
static osal_err_t counted_lock(osal_mutex_t *m, int64_t timeout);
static void counted_unlock(osal_mutex_t *m);
#define hal_fs_readdir counted_readdir
#define hal_fs_size counted_size
#define osal_mutex_lock counted_lock
#define osal_mutex_unlock counted_unlock
#ifndef INK_STORAGE_SOURCE
#define INK_STORAGE_SOURCE "../../system/services/file/file_service.c"
#endif
#include INK_STORAGE_SOURCE
#undef hal_fs_readdir
#undef hal_fs_size
#undef osal_mutex_lock
#undef osal_mutex_unlock
static int counted_readdir(hal_dir_t *d, hal_dirent_t *e) {
    reads++;
    return hal_fs_readdir(d, e);
}
static int64_t counted_size(const char *path) {
    bool content = measuring;
    uint64_t start = osal_now_us();
    if (content) { sizes++; locked_sizes += edit_owner == osal_task_self(); }
    if (content && edit_on_size && edit_owner != osal_task_self()) {
        void (*edit)(void) = edit_on_size; edit_on_size = NULL; edit();
    }
    int64_t n = hal_fs_size(path);
    if (content) size_us += osal_now_us() - start;
    return n;
}
static osal_err_t counted_lock(osal_mutex_t *m, int64_t timeout) {
    osal_err_t r = osal_mutex_lock(m, timeout);
    if (m == FS.edits && r == OSAL_OK) { edit_owner = osal_task_self(); hold_start = osal_now_us(); }
    return r;
}
static void counted_unlock(osal_mutex_t *m) {
    if (m == FS.edits) {
        uint64_t held = osal_now_us() - hold_start;
        if (measuring) { edit_holds++; if (held > max_hold_us) max_hold_us = held; }
        edit_owner = NULL;
    }
    osal_mutex_unlock(m);
}
void ink_test_storage_begin(void) { sizes = locked_sizes = edit_holds = 0; max_hold_us = size_us = 0; measuring = true; }
void ink_test_storage_end(void) {
    measuring = false;
    ink_probe_value("test.storage.edit_holds", edit_holds);
    ink_probe_value("test.storage.size_reads", sizes);
    ink_probe_value("test.storage.locked_size_reads", locked_sizes);
    ink_probe_value("test.storage.max_hold_us", (double)max_hold_us);
    ink_probe_value("test.storage.size_us", (double)size_us);
}
unsigned ink_test_storage_sizes(void) { return sizes; }
unsigned ink_test_storage_locked_sizes(void) { return locked_sizes; }
unsigned ink_test_storage_holds(void) { return edit_holds; }
void ink_test_storage_edit_on_size(void (*edit)(void)) { edit_on_size = edit; }
unsigned ink_test_storage_reads(void) { return reads; }
bool ink_test_storage_has_thumbs(void) { return FS.thumbs != NULL; }
