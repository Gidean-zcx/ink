#include "ink/sys/storage.h"
#include <string.h>
void ink_test_late_gate(const char *kind);
static void gate(const char *path) {
    if (!strcmp(path, "/user/m/oldadult/synthetic.state")) {
        ink_test_late_gate("app_write");
        ink_test_late_gate("app_remove");
    }
}
static int test_write(const char *path, const void *data, size_t len) {
    gate(path);
    return ink_storage_write_atomic(path, data, len);
}
static int test_member_write(const char *path, const void *data, size_t len, uint32_t generation) {
    gate(path);
    return ink_storage_write_member_atomic(path, data, len, generation);
}
static int test_remove(const char *path, ink_storage_done_fn done, void *ctx) {
    gate(path);
    return ink_storage_remove(path, done, ctx);
}
static int test_member_remove(const char *path, uint32_t generation, ink_storage_done_fn done, void *ctx) {
    gate(path);
    return ink_storage_remove_member(path, generation, done, ctx);
}
#define ink_storage_write_atomic test_write
#define ink_storage_write_member_atomic test_member_write
#define ink_storage_remove test_remove
#define ink_storage_remove_member test_member_remove
#ifndef INK_TEST_APP_SOURCE
#define INK_TEST_APP_SOURCE "../../system/ui/app_registry.c"
#endif
#include INK_TEST_APP_SOURCE

void ink_test_late_app_save(void) { save(find("synthetic")); }
