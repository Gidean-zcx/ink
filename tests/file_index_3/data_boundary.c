#include <string.h>
#include "ink/sys/storage.h"
#include "ink/sys/exec.h"
static unsigned appends, snapshots;
static bool probe_armed, probe_queued, probe_reached;
static ink_exec_job_t probe_job;
static void bg_probe(void *arg) { (void)arg; probe_reached = true; }
void ink_test_index_probe_on_append(void) { probe_armed = true; }
bool ink_test_index_probe_queued(void) { return probe_queued; }
bool ink_test_index_probe_reached(void) { return probe_reached; }
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
    if (!r && probe_armed && strstr(path, "/file.index.") && ink_exec_is_current(INK_EXEC_BG)) {
        probe_armed = false; probe_queued = true;
        ink_exec_post(INK_EXEC_BG, &probe_job, bg_probe, NULL);
    }
    return r;
}
static int counted_snapshot(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx) {
    int r = ink_storage_write_borrow(path, data, len, done, ctx);
    if (!r && strstr(path, "/file.index")) snapshots++;
    return r;
}
unsigned ink_test_index_writes(void) { return appends + snapshots; }
