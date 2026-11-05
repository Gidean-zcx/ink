#include "ink/hal_storage.h"
#include "ink/sys/file.h"
void ink_test_file_before_mkdir(const char *path);
bool ink_test_file_cleanup_entered, ink_test_file_cleanup_returned;
static int test_mkdir(const char *path) {
    ink_test_file_before_mkdir(path);
    return hal_fs_mkdir(path);
}
#define hal_fs_mkdir test_mkdir
#define ink_fs_forget_member real_forget_member
#ifndef INK_TEST_FILE_SOURCE
#define INK_TEST_FILE_SOURCE "../../system/services/file/file_service.c"
#endif
#include INK_TEST_FILE_SOURCE
#undef ink_fs_forget_member

int ink_fs_forget_member(const char *id) {
    ink_test_file_cleanup_entered = true;
    int r = real_forget_member(id);
    ink_test_file_cleanup_returned = true;
    return r;
}
