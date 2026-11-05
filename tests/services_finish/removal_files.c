#include <string.h>
#include "ink/hal_storage.h"
#include "ink/probe.h"
extern int removal_fail_card;
static hal_dir_t *removal_opendir(const char *path) {
    if (removal_fail_card && !strcmp(path, "/content/.m/oldadult")) {
        removal_fail_card = 0;
        ink_probe_count("review.card_io_failure");
        return NULL;
    }
    return hal_fs_opendir(path);
}
#define hal_fs_opendir removal_opendir
#ifndef INK_TEST_FILE_SOURCE
#define INK_TEST_FILE_SOURCE "../../system/services/file/file_service.c"
#endif
#include INK_TEST_FILE_SOURCE
#undef hal_fs_opendir
