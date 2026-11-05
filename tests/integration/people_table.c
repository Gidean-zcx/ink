/*
 * The data store unchanged, with the family's table not readable the first few times (tests/integration/people.c, INK_TEST_TABLE_FAILS).
 */
#include <stdlib.h>
#include <string.h>

#include "ink/sys/storage.h"

static int load_for_test(const char *path, void **out, size_t *len);
#define ink_storage_load load_for_test
#include "../../system/services/data/datastore.c"
#undef ink_storage_load

static int load_for_test(const char *path, void **out, size_t *len) {
    static int failed;
    const char *fails = getenv("INK_TEST_TABLE_FAILS");
    if (fails && strstr(path, "member.table") && strstr(path, ".log") && failed < atoi(fails)) {
        failed++;
        *out = NULL;
        *len = 0;
        return INK_STORAGE_LOAD_FAILED;
    }
    return ink_storage_load(path, out, len);
}
