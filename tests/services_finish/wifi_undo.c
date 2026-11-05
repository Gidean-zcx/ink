#include "product.h"
#include "ink/hal_net.h"
#include "ink/sys/crc.h"

static int results;
static char result_status[24];
static void result(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    ink_args_get_str(args, "status", result_status, sizeof result_status);
    results++;
}
static bool confirmed(const char *cap, const char *args) {
    if (!BY_HAND(cap, args) || g_rc.status != INK_RC_PENDING_CONFIRMATION) return false;
    unsigned token = 0;
    if (sscanf(g_rc.object, "confirm:%u", &token) != 1) return false;
    int before = results;
    return ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0 && WAIT_FOR(results > before, 5000) && !strcmp(result_status, "done");
}
static void add_aps(void *ctx) {
    (void)ctx;
    sim_wifi_ap_add("SYNTH_TARGET", -40, "");
    sim_wifi_ap_add("SYNTH_CONTROL", -40, "");
}
static uint32_t wid(const char *ssid) {
    uint32_t n = ink_crc32(0, ssid, strlen(ssid)) & 0x7fffffff;
    return n ? n : 1;
}
static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result", result, NULL, INK_EXEC_DIRECT) == 0, "listener");
    vtk_event_at(vtk_now() + SIM_MS(1), add_aps, NULL);
    delay_ms(20);
    CHECK(AI("wifi.connect", "{\"ssid\":\"SYNTH_CONTROL\"}") && g_rc.status == INK_RC_DONE && g_rc.revision > 0 && g_rc.undo_ms > 0, "saved new network is undoable");
    char undo[160];
    snprintf(undo, sizeof undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, g_rc.revision);
    CHECK(AI("system.undo", undo) && g_rc.status == INK_RC_UNDONE, "untouched saved network undo");
    CHECK(AI("wifi.list", NULL) && !strstr(g_result, "SYNTH_CONTROL"), "control credentials removed");
    ink_db_t *db = ink_db_open(ink_db_find_def("net.wifi"), NULL);
    for (int i = 0; i < 8; i++) {
        hal_wifi_known_t k = {0};
        snprintf(k.ssid, sizeof k.ssid, "SYNTH_OLD_%d", i);
        ink_db_write_t w = {.id = wid(k.ssid), .data = &k, .len = sizeof k, .src = INK_SRC_TOUCH};
        CHECK(db && ink_db_put(db, &w, NULL) == INK_DB_OK, "seed saved network %d", i);
    }
    CHECK(db && ink_db_count(db) == 8, "known network capacity reached");
    ink_db_close(db);
    CHECK(AI("wifi.connect", "{\"ssid\":\"SYNTH_TARGET\"}") && g_rc.status == INK_RC_DONE, "connect ninth network");
    CHECK(g_rc.revision == 0 && g_rc.undo_ms == 0, "failed save must not advertise an inverse");
    snprintf(undo, sizeof undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, g_rc.revision);
    CHECK(AI("wifi.list", NULL) && !strstr(g_result, "SYNTH_TARGET"), "unsaved target absent");
    CHECK(confirmed("wifi.forget", "{\"ssid\":\"SYNTH_OLD_0\"}"), "person frees a slot");
    CHECK(BY_HAND("wifi.connect", "{\"ssid\":\"SYNTH_TARGET\"}") && g_rc.status == INK_RC_DONE && g_rc.revision > 0, "person later saves target");
    CHECK(AI("wifi.list", NULL) && strstr(g_result, "SYNTH_TARGET"), "human credentials exist");
    CHECK(AI("system.undo", undo) && g_rc.status == INK_RC_FAILED, "old failed-save inverse rejected");
    CHECK(AI("wifi.list", NULL) && strstr(g_result, "SYNTH_TARGET"), "human credentials survive old inverse");
    CHECK(BY_HAND("wifi.connect", "{\"ssid\":\"SYNTH_TARGET\"}") && g_rc.status == INK_RC_DONE && !g_rc.undo_ms && g_rc.revision > 0, "existing network update has no deletion inverse");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "review3-wifi", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
