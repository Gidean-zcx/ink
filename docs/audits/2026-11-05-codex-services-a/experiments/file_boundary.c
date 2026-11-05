/* Product source unchanged. One HAL open failure; one scheduling boundary after job submission. */
#include <string.h>
#include <stdlib.h>
#include "ink/hal_storage.h"
#include "ink/sys/res.h"
#include "ink/sys/file.h"
#include "ink/osal.h"
int audit_fail_root;
int audit_pause_submit;
static hal_dir_t *audit_opendir(const char *path) {
    static int env_used;
    if ((audit_fail_root || (!env_used && getenv("AUDIT_FAIL_CARD_ROOT"))) && !strcmp(path, "/content/.m")) {
        audit_fail_root = 0;
        env_used = 1;
        return NULL;
    }
    return hal_fs_opendir(path);
}
static int audit_submit(const ink_job_t *job) {
    int r = ink_job_submit(job);
    if (audit_pause_submit && r > 0) {
        audit_pause_submit = 0;
        osal_task_delay_us(500000);
    }
    return r;
}
#define hal_fs_opendir audit_opendir
#define ink_job_submit audit_submit
#include "../../../../system/services/file/file_service.c"
#undef hal_fs_opendir
#undef ink_job_submit
int audit_nopen(void) { return FS.nopen; }
int audit_file_task(void) { return FS.job.task; }
