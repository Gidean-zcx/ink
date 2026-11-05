/* Original product TU. State-write failure models a full storage queue; no state machine edits. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ink/sys/storage.h"
static int audit_state_write(const char *path, const void *data, size_t len) {
    if (getenv("AUDIT_FAIL_STATE") && !strcmp(path, "/user/sys/update.json")) {
        printf("AUDIT refused_state_write bytes=%zu\n", len);
        return -1;
    }
    return ink_storage_write_atomic(path, data, len);
}
#define ink_storage_write_atomic audit_state_write
#include "../../../../system/services/update/update_service.c"
#undef ink_storage_write_atomic
int audit_package(const char *path) {
    pkg_t p;
    return (int)check_package(path, &p);
}
int audit_dl_offset(void) { return (int)U.dl_off; }
int audit_dl_job(void) { return U.job; }
int audit_dl_task(void) { return U.task; }
int audit_dl_waiting(void) { return U.dl_waiting; }
void audit_update_dump(void) {
    printf("AUDIT update runtime_code=%u state=%s code=%u res=%u path=%s blocked=%u offer=%u dl_off=%u job=%d\n",
           U.me.version_code, U.st.state, U.st.code, U.st.res_done_section, U.st.path, U.st.blocked_code, U.offer_code, U.dl_off, U.job);
}
