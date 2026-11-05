/* Faults and scheduling at the existing service's dependencies. */
#include <string.h>
#include "ink/hal_storage.h"
#include "ink/sys/res.h"
#include "ink/osal.h"
int ink_test_fail_root, ink_test_pause_submit;
static hal_dir_t *test_opendir(const char *path) {
    if (ink_test_fail_root && !strcmp(path, "/content/.m")) {
        ink_test_fail_root = 0;
        return NULL;
    }
    return hal_fs_opendir(path);
}
static int test_submit(const ink_job_t *job) {
    int r = ink_job_submit(job);
    if (ink_test_pause_submit && r > 0) {
        ink_test_pause_submit = 0;
        osal_task_delay_us(500000);
    }
    return r;
}
#define hal_fs_opendir test_opendir
#define ink_job_submit test_submit
#ifndef INK_TEST_FILE_SOURCE
#define INK_TEST_FILE_SOURCE "../../system/services/file/file_service.c"
#endif
#include INK_TEST_FILE_SOURCE
