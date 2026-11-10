#include <stdlib.h>
#include "ink/sys/interrupt.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "common.h"

int ink_shell_test_effects;
static char scene[16];
void ink_test_shell_confirm(void);
void ink_test_shell_interrupt(void);
void ink_test_shell_power(void);
void ink_test_shell_member(void);
void ink_test_shell_lock(void);
void ink_test_shell_off(void);
static void effect(ink_call_t *c) { (void)c; ink_shell_test_effects++; }
static const ink_cap_t CAPS[] = {
    {.id = "test.shell_action", .title = "合成动作", .child_ok = true, .effect = INK_FX_LOCAL_WRITE, .run = effect},
    {.id = "test.shell_confirm", .title = "合成确认", .ask = "合成内容", .child_ok = true, .effect = INK_FX_EXTERNAL, .run = effect}
};
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR((on_ui(read_screen), g_top[0] && settled()), 15000), "home settles");
    test_start_home();
    CHECK(WAIT_FOR(settled(), 15000), "feature home settles");
    ink_power_hold(INK_NEED_AWAKE);
    if (!strcmp(scene, "confirm")) {
        CHECK(AI("test.shell_confirm", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "real synthetic request waits for a human");
        CHECK(WAIT_FOR(settled(), 15000), "confirmation is presented");
        on_ui(ink_test_shell_confirm);
    } else if (!strcmp(scene, "interrupt")) {
        ink_interrupt_t it = {.kind = INK_INT_CALL, .nactions = 1, .actions = {{.label = "合成动作", .cap = "test.shell_action"}}};
        snprintf(it.title, sizeof it.title, "合成打断"); snprintf(it.body, sizeof it.body, "合成内容");
        int id = ink_interrupt_raise(&it);
        CHECK(id > 0 && WAIT_FOR(!strcmp(active_state(), "call") && settled(), 15000), "real synthetic interruption is presented");
        on_ui(ink_test_shell_interrupt); ink_interrupt_end(id);
    } else if (!strcmp(scene, "power")) on_ui(ink_test_shell_power);
    else if (!strcmp(scene, "member")) on_ui(ink_test_shell_member);
    else if (!strcmp(scene, "lock")) on_ui(ink_test_shell_lock);
    else if (!strcmp(scene, "off")) on_ui(ink_test_shell_off);
    else CHECK(false, "known scene");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    sim_copy_name(scene, sizeof scene, getenv("INK_SHELL_SCENE"));
    product_main();
    CHECK(ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) == 0, "register synthetic capabilities");
    osal_task_cfg_t cfg = {.name = "shell-semantics-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start shell semantics driver");
}
