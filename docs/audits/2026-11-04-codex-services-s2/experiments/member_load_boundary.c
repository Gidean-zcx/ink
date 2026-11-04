/* Product datastore unchanged; fail one real member-table load at the storage API boundary. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/sys/storage.h"
static int audit_load(const char *, void **, size_t *);
#define ink_storage_load audit_load
#include "../../../../system/services/data/datastore.c"
#undef ink_storage_load

static int audit_load(const char *path, void **out, size_t *len) {
    static int failed;
    if (!failed && getenv("AUDIT_MEMBER_LOAD_FAIL") && strstr(path, "member.table") && strstr(path, ".log")) {
        failed = 1;
        *out = NULL;
        *len = 0;
        printf("AUDIT injected member-table load failure\n");
        return INK_STORAGE_LOAD_FAILED;
    }
    return ink_storage_load(path, out, len);
}
