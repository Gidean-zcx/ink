/* Touch targets and AI calls use the same existing mutation capabilities. */
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

#define WIFI_ARGS "{\"ssid\":\"SYNTH_\\\"wifi\\\\test\"}"
static char snapshot[4096], scene[32], binding[48];
static const char *wanted;
static int x, y;
static lv_obj_t *found;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static void read_page(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "page exports snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "page fits production AI context");
}
static bool page_has(const char *text) { on_ui(read_page); return strstr(snapshot, text) != NULL; }
static void find_at(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted)) found = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_at(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) {
    found = NULL; x = y = -1; binding[0] = 0;
    lv_obj_update_layout(lv_screen_active()); find_at(lv_screen_active()); find_at(lv_layer_sys());
    if (!found) return;
    lv_area_t a, clip; lv_obj_get_coords(found, &a);
    for (lv_obj_t *p = found; p; p = lv_obj_get_parent(p)) {
        if (lv_obj_has_state(p, LV_STATE_DISABLED)) return;
        lv_obj_get_coords(p, &clip);
        if (a.x1 < clip.x1 || a.x2 > clip.x2 || a.y1 < clip.y1 || a.y2 > clip.y2) return;
        const char *cap = ink_shell_bound_cap(p);
        if (cap) snprintf(binding, sizeof binding, "%s", cap);
    }
    x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
}
static bool target(const char *label) {
    wanted = label; on_ui(locate);
    for (int i = 0; x < 0 && i < 3 && !strcmp(ink_ui_page_name(), "SetNetwork"); i++) {
        CHECK(AI("shell.page_next", NULL), "visit network settings page"); quiet(); on_ui(locate);
    }
    return x >= 0 && y >= 0 && y < INK_CANVAS_H;
}
static void press(const char *label) {
    CHECK(target(label), "visible touch target: %s", label);
    if (x >= 0) tap(x, y);
    quiet();
}
static void save(void *arg) {
    (void)arg; char path[600];
    int n = snprintf(path, sizeof path, "%s/%s.txt", g_sim.out_dir, scene);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb");
    if (!f || fputs(snapshot, f) == EOF || fclose(f)) abort();
}
static void capture(void *arg) { sim_display_snapshot(arg); }
static void evidence(const char *name) {
    snprintf(scene, sizeof scene, "%s", name); quiet(); on_ui(read_page);
    vtk_host_call(save, NULL); vtk_event_at(vtk_now() + SIM_MS(1), capture, scene); delay_ms(5);
}
static void add_ap(void *arg) { (void)arg; sim_wifi_ap_add("SYNTH_\"wifi\\test", -40, ""); }
static bool has_confirmation(const char *cap, ink_source_t src) {
    ink_confirm_view_t cards[4]; int n = ink_confirm_list(cards, 4);
    return n == 1 && !strcmp(cards[0].cap, cap) && cards[0].src == src;
}
static void answer(void) {
    CHECK(WAIT_FOR(({ on_ui(read_screen); !strcmp(g_top, "confirm") && strstr(g_snap, "页面=Confirm"); }), 5000), "existing confirmation card opens");
    ink_confirm_view_t card = {0}; char args[64];
    CHECK(ink_confirm_list(&card, 1) == 1, "confirmation has an explicit target");
    snprintf(args, sizeof args, "{\"token\":%u}", (unsigned)card.token);
    CHECK(AI("confirm.accept", args) && g_rc.status == INK_RC_INVALID, "AI cannot confirm deletion");
    CHECK(WAIT_FOR(target("删除"), 5000), "human confirmation target appears"); press("删除");
}
static void wifi(void) {
    vtk_event_at(vtk_now() + SIM_MS(1), add_ap, NULL); delay_ms(5);
    CHECK(AI("wifi.connect", WIFI_ARGS) && g_rc.status == INK_RC_DONE, "remember synthetic network");
    CHECK(AI("shell.network", NULL) && g_rc.status == INK_RC_DONE, "open network settings"); quiet();
    CHECK(page_has("wifi.forget(" WIFI_ARGS ")"), "Wi-Fi snapshot carries the exact escaped capability arguments");
    CHECK(target("忘掉这个网络") && !strcmp(binding, "wifi.forget"), "Wi-Fi touch target binds the existing capability");
    evidence("wifi"); press("忘掉这个网络");
    CHECK(has_confirmation("wifi.forget", INK_SRC_TOUCH), "touch preserves Wi-Fi confirmation policy");
    CHECK(AI("wifi.list", NULL) && strstr(g_result, "SYNTH_"), "network survives before confirmation");
    answer();
    CHECK(WAIT_FOR(({ AI("wifi.list", NULL) && !strstr(g_result, "SYNTH_"); }), 5000), "touch removes saved network after confirmation");
    CHECK(!page_has("wifi.forget("), "forgotten network no longer has an action");
    CHECK(AI("wifi.connect", WIFI_ARGS) && g_rc.status == INK_RC_DONE, "remember network again");
    CHECK(AI("wifi.forget", WIFI_ARGS) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI uses identical Wi-Fi capability and policy");
    CHECK(has_confirmation("wifi.forget", INK_SRC_AI), "AI request names the same capability"); answer();
    CHECK(WAIT_FOR(({ AI("wifi.list", NULL) && !strstr(g_result, "SYNTH_"); }), 5000), "AI forget produces the same result");
}
static void notifications(void) {
    CHECK(AI("notify.post", "{\"title\":\"SYNTH_NOTICE\"}") && g_rc.status == INK_RC_DONE, "seed notification");
    CHECK(AI("shell.notifications", NULL) && g_rc.status == INK_RC_DONE, "open notifications"); quiet();
    CHECK(page_has("\"capability\":\"notify.clear\""), "notification snapshot exposes clear");
    CHECK(target("全部清除") && !strcmp(binding, "notify.clear"), "clear touch target binds the existing capability");
    evidence("notifications"); press("全部清除");
    CHECK(has_confirmation("notify.clear", INK_SRC_TOUCH) && ink_notify_unread() > 0, "touch clear waits for confirmation"); answer();
    CHECK(WAIT_FOR(ink_notify_unread() == 0, 5000), "touch clear removes notifications");
    CHECK(page_has("\"total\":0"), "notification list refreshes after clearing");
    CHECK(AI("notify.post", "{\"title\":\"SYNTH_NOTICE\"}"), "seed notification again");
    CHECK(AI("notify.clear", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI clear preserves the same confirmation");
    CHECK(has_confirmation("notify.clear", INK_SRC_AI), "AI request names the same clear capability"); answer();
    CHECK(WAIT_FOR(ink_notify_unread() == 0, 5000), "AI clear produces the same result");
}
static bool file_exists(const char *path) { ink_fs_stat_t s; return ink_fs_stat(path, &s) == 0 && s.size == 10; }
static void files(void) {
    CHECK(ink_fs_mkdir("/me/SYNTH_DEST") == 0, "create copy destination");
    CHECK(ink_fs_write_atomic("/me/SYNTH_SOURCE.txt", "SYNTH_DATA", 10) == 0, "create copy source");
    uint32_t cursor = 0, id = 0; ink_fs_entry_t e;
    while (ink_fs_index("/me", INK_FS_ANY, &cursor, &e) == 1) if (!strcmp(e.path, "/me/SYNTH_SOURCE.txt")) id = e.id;
    CHECK(id != 0, "copy source has an indexed identifier");
    CHECK(AI("app.open", "{\"id\":\"files\"}") && g_rc.status == INK_RC_DONE, "open file app"); quiet();
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id);
    CHECK(AI("files.select", args) && g_rc.status == INK_RC_DONE, "select source file");
    CHECK(WAIT_FOR(page_has("\"id\":\"copy\""), 5000), "file actions snapshot exposes copy picker"); quiet();
    evidence("file-actions"); press("拷贝");
    CHECK(!strcmp(ink_ui_page_name(), "FileMove"), "copy reuses the designed destination picker");
    CHECK(AI("files.browse", "{\"path\":\"/me/SYNTH_DEST\"}") && g_rc.status == INK_RC_DONE, "choose destination");
    CHECK(WAIT_FOR(page_has("\"capability\":\"file.copy\""), 5000), "destination snapshot exposes the existing copy capability"); quiet();
    CHECK(strstr(snapshot, "/me/SYNTH_SOURCE.txt") && strstr(snapshot, "/me/SYNTH_DEST/SYNTH_SOURCE.txt"), "copy snapshot identifies both paths");
    evidence("file-copy"); press("拷贝到这里");
    CHECK(WAIT_FOR(file_exists("/me/SYNTH_DEST/SYNTH_SOURCE.txt"), 5000), "touch executes file.copy");
    CHECK(file_exists("/me/SYNTH_SOURCE.txt"), "copy preserves its source");
    CHECK(AI("file.copy", "{\"from\":\"/me/SYNTH_SOURCE.txt\",\"to\":\"/me/SYNTH_DEST/SYNTH_AI.txt\"}") && g_rc.status == INK_RC_DONE, "AI calls the same file.copy capability");
    CHECK(WAIT_FOR(file_exists("/me/SYNTH_DEST/SYNTH_AI.txt"), 5000), "AI copy produces the same result");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); quiet();
    wifi(); notifications(); files();
    CHECK(WAIT_FOR(ink_storage_pending() == 0, 10000), "writes settle");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "settings-gaps", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("settings gaps test");
}
