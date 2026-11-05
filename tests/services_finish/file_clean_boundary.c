#ifndef INK_TEST_FILE_SOURCE
#define INK_TEST_FILE_SOURCE "../../system/services/file/file_service.c"
#endif
#include INK_TEST_FILE_SOURCE
#ifdef INK_TEST_OLD_FILE
uint32_t ink_fs_file_revision(ink_fs_file_t *f) { (void)f; return 1; }
int ink_fs_remove_if_revision(const char *path, uint32_t rev) { (void)rev; return ink_fs_remove(path); }
int ink_fs_move_if_revision(const char *a, const char *b, uint32_t rev, uint32_t *out) { (void)rev; if (out) *out=1; return ink_fs_move(a,b); }
#endif
