/* Pause real card boundaries; never change a path or a card result. */
#include <string.h>
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/sys/cap.h"

extern int ink_test_file_gate_at, ink_test_file_gate_kind;
extern bool ink_test_file_held, ink_test_file_release;
static void gate(void) {
    ink_test_file_held = true;
    while (!ink_test_file_release) osal_task_delay_us(10000);
}
static bool test_card_present(const char *caller) {
    bool present = hal_fs_card_present();
    if (!ink_test_file_gate_kind && ink_test_file_gate_at > 0 &&
        (!strcmp(caller, "fs_resolve") || !strcmp(caller, "resolve_for")) && --ink_test_file_gate_at == 0) gate();
    return present;
}
static hal_dir_t *test_opendir(const char *path, const char *caller) {
    hal_dir_t *dir = hal_fs_opendir(path);
    if (ink_test_file_gate_kind == 1 && ink_test_file_gate_at > 0 && !strcmp(caller, "job_begin_unlocked")) {
        ink_test_file_gate_at = 0;
        gate();
    }
    return dir;
}
static int64_t test_size(const char *path, const char *caller) {
    int64_t size = hal_fs_size(path);
    if (ink_test_file_gate_kind == 3 && ink_test_file_gate_at > 0 && !strcmp(caller, "fs_open_unlocked")) {
        ink_test_file_gate_at = 0;
        gate();
    }
    return size;
}
static void before_emit(const char *id, const ink_args_t *args) {
    if (ink_test_file_gate_kind != 2 || ink_test_file_gate_at <= 0 || strcmp(id, "file.changed")) return;
    char path[128] = "";
    ink_args_get_str(args, "path", path, sizeof path);
    if (!strstr(path, "SYNTH_OWNER_PRIVATE")) return;
    ink_test_file_gate_at = 0;
    gate();
}
static void test_emit(const char *id, const ink_args_t *args) {
    before_emit(id, args);
    ink_cap_emit(id, args);
}
static void test_emit_for(const char *id, const ink_args_t *args, const char *member) {
    before_emit(id, args);
    ink_cap_emit_for(id, args, member);
}
#define hal_fs_card_present() test_card_present(__func__)
#define hal_fs_opendir(path) test_opendir(path, __func__)
#define hal_fs_size(path) test_size(path, __func__)
#define ink_cap_emit test_emit
#define ink_cap_emit_for test_emit_for
#ifndef INK_TEST_FILE_SOURCE
#define INK_TEST_FILE_SOURCE "../../system/services/file/file_service.c"
#endif
#include INK_TEST_FILE_SOURCE
#undef hal_fs_card_present
#undef hal_fs_opendir
#undef hal_fs_size
#undef ink_cap_emit
#undef ink_cap_emit_for
#ifndef INK_TEST_FILE_CAPS_SOURCE
#define INK_TEST_FILE_CAPS_SOURCE "../../system/services/file/file_caps.c"
#endif
#include INK_TEST_FILE_CAPS_SOURCE
