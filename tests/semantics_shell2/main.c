#include <stdlib.h>
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "common.h"
static char scene[16];
bool ink_test_shell2_native_output;
void ink_test_shell2_keyboard(void);
void ink_test_shell2_ai(void);
void ink_test_shell2_member(void);
void ink_test_shell2_kid(void);
void ink_test_shell2_battery(void);
void ink_test_shell2_home(void);
extern bool ink_test_shell2_note_left;
void ink_test_shell2_note_check(void);
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR((on_ui(read_screen), g_top[0] && settled()), 15000), "home settles");
    test_start_home();
    CHECK(WAIT_FOR(settled(), 15000), "feature home settles");
    ink_power_hold(INK_NEED_AWAKE);
    if (!strcmp(scene, "keyboard")) on_ui(ink_test_shell2_keyboard);
    else if (!strcmp(scene, "ai")) on_ui(ink_test_shell2_ai);
    else if (!strcmp(scene, "member")) on_ui(ink_test_shell2_member);
    else if (!strcmp(scene, "kid")) on_ui(ink_test_shell2_kid);
    else if (!strcmp(scene, "battery")) on_ui(ink_test_shell2_battery);
    else if (!strcmp(scene, "home")) {
        on_ui(ink_test_shell2_home);
        uint64_t set_at = osal_now_us();
        CHECK(WAIT_FOR((on_ui(ink_test_shell2_note_check), !ink_test_shell2_note_left), 6000) &&
              osal_now_us() - set_at >= 3500000, "the note clears itself after about 4 s");
    }
    else CHECK(false, "known scene");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    sim_copy_name(scene, sizeof scene, getenv("INK_SHELL_SCENE"));
    product_main();
    osal_task_cfg_t cfg = {.name = "shell2-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start shell2 semantics driver");
}
