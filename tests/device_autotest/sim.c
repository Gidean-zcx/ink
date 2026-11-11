#include <stdlib.h>
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include "ink/sys/textinput.h"
static ink_exec_job_t check_job;
static char scene[16];
static volatile int checked;
static void options(void *unused) {
    (void)unused; const char *value = getenv("INK_AUTOTEST_SCENE");
    sim_copy_name(scene, sizeof scene, value ? value : "cap");
}
static void labels(lv_obj_t *obj) {
    if (lv_obj_check_type(obj, &lv_label_class)) {
        lv_area_t a; lv_obj_get_coords(obj, &a);
        sim_note("test label %.20s at %d %d", lv_label_get_text(obj), (int)((a.x1 + a.x2) / 2), (int)((a.y1 + a.y2) / 2));
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) labels(lv_obj_get_child(obj, i));
}
static void check(void *arg) {
    int stage = (int)(intptr_t)arg;
    if (!strcmp(scene, "memo")) {
        if (stage == 1) {
            CHECK(ink_text_focused() && !strcmp(ink_text_value(), "hello"), "keyboard taps type hello: %.40s composing %.40s", ink_text_value(), ink_text_composing());
            if (strcmp(ink_text_value(), "hello")) labels(lv_layer_sys());
        } else if (stage == 2) CHECK(!ink_text_value()[0], "backspace erases the new draft text: %.40s", ink_text_value());
        else CHECK(!strcmp(ink_ui_page_name(), "home"), "memo script returns home");
    } else if (!strcmp(scene, "reader")) {
        if (stage == 1) CHECK(!strcmp(ink_ui_page_name(), "book"), "reader script opens the synthetic book");
        else CHECK(!strcmp(ink_ui_page_name(), "home"), "reader script returns home");
    } else if (!strcmp(scene, "camera") && stage == 2) CHECK(!strcmp(ink_ui_page_name(), "home"), "camera script returns home");
    else CHECK(!strcmp(ink_ui_page_name(), "Camera"), "script cap opens camera through real capability bus: %.40s", ink_ui_page_name());
    __atomic_store_n(&checked, 1, __ATOMIC_RELEASE);
}
static void at(unsigned seconds, int stage) {
    uint64_t target = (uint64_t)seconds * 1000000;
    if (target > osal_now_us()) osal_task_delay_us(target - osal_now_us());
    __atomic_store_n(&checked, 0, __ATOMIC_RELEASE);
    ink_exec_post(INK_EXEC_UI, &check_job, check, (void *)(intptr_t)stage);
    while (!__atomic_load_n(&checked, __ATOMIC_ACQUIRE)) osal_task_delay_us(1000);
}
static void driver(void *unused) {
    (void)unused; test_start_home();
    if (!strcmp(scene, "memo")) { at(21, 1); at(27, 2); at(32, 3); } /* memo.script waits 4 s after the language switch */
    else if (!strcmp(scene, "reader")) { at(14, 1); at(55, 2); }
    else if (!strcmp(scene, "camera")) { at(10, 1); at(28, 2); }
    else at(10, 0);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(options, NULL); product_main();
    const osal_task_cfg_t cfg = {.name = "autotest-check", .fn = driver, .stack_bytes = 8192,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start capability check");
}
