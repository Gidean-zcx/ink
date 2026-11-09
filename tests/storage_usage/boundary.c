#include "ink/hal_storage.h"
static unsigned reads, organizing_reads;
static int counted_readdir(hal_dir_t *d, hal_dirent_t *e);
#define hal_fs_readdir counted_readdir
#ifndef INK_STORAGE_SOURCE
#define INK_STORAGE_SOURCE "../../system/services/file/file_service.c"
#endif
#include INK_STORAGE_SOURCE
#undef hal_fs_readdir
static int counted_readdir(hal_dir_t *d, hal_dirent_t *e) {
    char state[32] = "";
    reads++;
    ink_state_get_str(ink_state_find("file.index_state"), state, sizeof state);
    if (!strcmp(state, "organizing")) organizing_reads++;
    return hal_fs_readdir(d, e);
}
unsigned ink_test_storage_reads(void) { return reads; }
unsigned ink_test_storage_organizing(void) { return organizing_reads; }
