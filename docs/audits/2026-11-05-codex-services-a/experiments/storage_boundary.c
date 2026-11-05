/* Original storage TU; a directory read failure only while wipe_dir walks /user. */
#include <stdlib.h>
#include <string.h>
#include "ink/hal_storage.h"
static hal_dir_t *target;
static hal_dir_t *audit_storage_open(const char *path) {
    hal_dir_t *d=hal_fs_opendir(path);
    if(getenv("AUDIT_FAIL_USER_READDIR") && !strcmp(path,"/user")) target=d;
    return d;
}
static int audit_storage_next(hal_dir_t *d,hal_dirent_t *e) {
    if(target && d==target) { target=NULL;return -1; }
    return hal_fs_readdir(d,e);
}
#define hal_fs_opendir audit_storage_open
#define hal_fs_readdir audit_storage_next
#include "../../../../system/services/storage/storage_service.c"
#undef hal_fs_opendir
#undef hal_fs_readdir
