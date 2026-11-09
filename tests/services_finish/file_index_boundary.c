#ifndef INK_TEST_FILE_SOURCE
#define INK_TEST_FILE_SOURCE "../../system/services/file/file_service.c"
#endif
#include INK_TEST_FILE_SOURCE
unsigned ink_test_file_index_count(void) {
    ink_db_t *db = ink_db_open(&INDEX_DEF, NULL);
    unsigned count = db ? (unsigned)ink_db_count(db) : 0;
    ink_db_close(db); return count;
}
#ifndef INK_TEST_FILE_CAPS_SOURCE
#define INK_TEST_FILE_CAPS_SOURCE "../../system/services/file/file_caps.c"
#endif
#include INK_TEST_FILE_CAPS_SOURCE
