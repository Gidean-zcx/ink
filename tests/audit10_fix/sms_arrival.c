/* Keep the real SMS page open while the AT model receives a message. */
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static const char BODY[] = "SYNTH_SMS_ARRIVAL";
static char page[4096];
static bool visible;
static void labels(lv_obj_t *obj) {
    if (!lv_obj_is_visible(obj)) return;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), BODY)) visible = true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) labels(lv_obj_get_child(obj, (int32_t)i));
}
static void inspect(void) {
    CHECK(ink_ui_snapshot(page, sizeof page, NULL) > 0, "SMS page snapshot fits");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "SMS production AI context fits");
    lv_obj_update_layout(lv_screen_active());
    visible = false;
    labels(lv_screen_active());
}
static bool stored(void) {
    ink_db_t *db = ink_db_open_member(ink_db_find_def("sms.messages"), "owner");
    bool ready = db && ink_db_count(db) == 1 && ink_db_durable_ver(db) == ink_db_last_ver(db);
    ink_db_close(db);
    return ready;
}
static void incoming(void *arg) {
    (void)arg;
    CHECK(sim_net_deliver_sms("00000000001", BODY), "AT model accepts synthetic SMS");
}
static void capture(void *arg) { (void)arg; sim_display_snapshot("sms-arrived"); }
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home();
    CHECK(WAIT_FOR(sim_net_modem_registered(), 10000), "AT modem registers");
    CHECK(BY_HAND("shell.launch", "{\"id\":\"message\"}") && g_rc.status == INK_RC_DONE, "open SMS once");
    CHECK(WAIT_FOR(({ on_ui(inspect); strstr(page, "\"total\":0"); }), 10000), "empty SMS list loads");
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "empty SMS page settles");
    vtk_event_at(vtk_now() + SIM_MS(1), incoming, NULL);
    CHECK(WAIT_FOR(stored() && ink_db_idle() && !ink_storage_pending(), 10000), "incoming SMS is durable");
    CHECK(WAIT_FOR(({ on_ui(inspect); visible && strstr(page, BODY) && strstr(g_snap, BODY); }), 10000),
        "arrived SMS appears on screen and in snapshots without reopening");
    CHECK(!strcmp(ink_ui_page_name(), "Sms") && strstr(page, "\"total\":1"), "same SMS page shows one conversation");
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "updated SMS page settles");
    vtk_event_at(vtk_now() + SIM_MS(1), capture, NULL);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "sms-arrival-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start SMS arrival test");
}
