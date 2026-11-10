/* Three elapsed call minutes, with minute notifications and no full redraw. */
#include <stdlib.h>
#ifndef INK_GAPS_PHONE_SOURCE
#define INK_GAPS_PHONE_SOURCE "../../system/apps/phone/phone.c"
#endif
#include INK_GAPS_PHONE_SOURCE
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static lv_obj_t *test_duration;
static uint32_t test_version, test_generation;
static int test_minute, test_lang;
static char test_snap[4096];
static bool test_settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void test_read(void) {
    CHECK(ink_ui_snapshot(test_snap, sizeof test_snap, NULL) > 0 && ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "call duration has a complete AI snapshot");
    char expected[32]; snprintf(expected, sizeof expected, test_lang ? "%d min" : "%d 分钟", test_minute);
    CHECK(P.duration && !strcmp(lv_label_get_text(P.duration), expected) && strstr(test_snap, expected), "elapsed minutes are visible to user and AI: %s", expected);
    CHECK(test_duration == P.duration && test_generation == ink_ui_generation(), "minute change preserves the page and its duration widget");
}
static void test_tick(void) { ink_state_set_int(ink_state_find("time.minute"), 700 + test_minute * 2); }
static void test_same_minute(void) {
    CHECK(ink_ui_content_version() == test_version, "another minute notification within the same elapsed minute does not refresh call content");
    test_read();
}
static void test_begin(void) { test_duration = P.duration; test_generation = ink_ui_generation(); test_read(); }
static void test_version_read(void) { test_version = ink_ui_content_version(); }
static void test_env(void *ctx) { (void)ctx; test_lang = getenv("INK_GAPS_EN") != NULL; }
static void test_frame(void *ctx) { (void)ctx; sim_display_snapshot("call-3-minutes"); }
static void run(void *ctx) {
    (void)ctx; vtk_host_call(test_env, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home();
    if (test_lang) CHECK(AI("settings.set", "{\"id\":\"system.language\",\"value\":1}"), "select English duration");
    CHECK(BY_HAND("phone.open", NULL), "open phone");
    CHECK(BY_HAND("call.dial", "{\"number\":\"00000000001\"}") && g_rc.status == INK_RC_DONE, "dial synthetic number through modem capability");
    CHECK(WAIT_FOR(H.active && H.answered && P.started && P.duration && test_settled(), 12000), "modem connects and call page settles");
    on_ui(test_begin);
    for (test_minute = 1; test_minute <= 3; test_minute++) {
        CHECK(WAIT_FOR(ink_now_us() >= P.started + (uint64_t)test_minute * 60000000 + 100000, 61000), "simulate another elapsed minute");
        on_ui(test_tick); delay_ms(60); on_ui(test_read); on_ui(test_version_read);
        ink_state_set_int(ink_state_find("time.minute"), 701 + test_minute * 2); delay_ms(60); on_ui(test_same_minute);
    }
    CHECK(WAIT_FOR(test_settled(), 10000), "three-minute frame settles"); vtk_event_at(vtk_now() + SIM_MS(1), test_frame, NULL); delay_ms(10);
    CHECK(BY_HAND("call.hangup", NULL) && g_rc.status == INK_RC_DONE, "hang up through existing capability");
    CHECK(WAIT_FOR(!H.active && ink_db_idle(), 10000), "call history is written"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "small-gaps-phone", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start minute acceptance test");
}
