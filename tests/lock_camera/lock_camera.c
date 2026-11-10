/* Real lock surfaces, touch driver and capabilities; synthetic photos and PIN only. */
#include <stdlib.h>
#include "../../system/ui/shell/member_pages.h"
#include "../../system/apps/camera/camera.c"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char phase[12];
static lv_point_t point;
static const char *wanted;
static bool found;
static bool frame_ready;
static uint32_t old_id;
static volatile int pin_done, pin_result;
static volatile int photo_done;
static ink_rc_t photo_status;
static const char *shot_name;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy() && !__atomic_load_n(&taking, __ATOMIC_ACQUIRE); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "UI and capture settle"); }
static void told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx; (void)wait_s; pin_result = result; pin_done++;
}
static void saved(void *ctx, const ink_receipt_t *rc) {
    (void)ctx; photo_status = rc->status; __atomic_store_n(&photo_done, 1, __ATOMIC_RELEASE);
}
static void ok(const char *cap, const char *args) {
    CHECK(BY_HAND(cap, args) && g_rc.status == INK_RC_DONE, "%s succeeds: %s %.120s", cap, ink_rc_name(g_rc.status), g_rc.detail);
    quiet();
}
static void locate(lv_obj_t *root) {
    if (!lv_obj_is_visible(root)) return;
    const char *cap = ink_shell_bound_cap(root);
    if (cap && !strcmp(cap, wanted)) {
        lv_area_t a; lv_obj_get_coords(root, &a);
        point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; found = true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) locate(lv_obj_get_child(root, (int32_t)i));
}
static void find(void) { found = false; lv_obj_update_layout(lv_layer_sys()); locate(lv_layer_sys()); }
static void press(const char *cap) {
    wanted = cap; on_ui(find); CHECK(found, "lock surface has %s", cap);
    if (found) {
        sim_input_report(); double before = sim_metric_value("input.touches");
        tap(point.x, point.y);
        sim_input_report();
        CHECK(WAIT_FOR(sim_metric_value("input.touches") > before, 2000), "touch reached the driver");
    }
    quiet();
}
static unsigned count(const char *folder) {
    uint32_t cursor = 0; ink_fs_entry_t e; unsigned n = 0;
    while (ink_fs_index(folder, INK_FS_IMAGE, &cursor, &e) == 1) n++;
    return n;
}
static void inspect(void) {
    read_screen();
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "production-sized AI snapshot fits");
    if (ink_camera_restricted()) {
        CHECK(strstr(g_snap, "camera.photo") && !strstr(g_snap, "/") && !strstr(g_snap, "file:") &&
              !strstr(g_snap, "以前") && !strstr(g_snap, "owner") && !strstr(g_snap, "1111"), "restricted snapshot has no paths, old photos or personal data: %.150s", g_snap);
        CHECK(view && lv_obj_get_child_count(view) == 1, "restricted view presents a real camera frame or saved image");
    }
}
static void frame(void) { frame_ready = view && lv_obj_get_child_count(view) == 1; }
static void snapshot_event(void *arg) { (void)arg; sim_display_snapshot(shot_name); }
static void capture(const char *name) {
    quiet(); shot_name = name; vtk_event_at(vtk_now() + SIM_MS(1), snapshot_event, NULL); delay_ms(5);
}
static bool click_label(lv_obj_t *root, const char *label) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *c = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_check_type(c, &lv_label_class) && !strcmp(lv_label_get_text(c), label) && lv_obj_check_type(root, &lv_button_class)) {
            lv_obj_send_event(root, LV_EVENT_CLICKED, NULL); return true;
        }
        if (click_label(c, label)) return true;
    }
    return false;
}
static void open_pin(void) { ink_shell_member_pin_open("owner"); }
static void enter_pin(void) {
    for (int i = 0; i < 4; i++) CHECK(click_label(lv_layer_sys(), "1"), "person enters a synthetic PIN digit on the device");
    CHECK(click_label(lv_layer_sys(), "确定"), "person submits the device PIN");
}
static void blocked(void) {
    char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)old_id);
    const char *caps[] = {"photo.open", "file.lookup", "file.thumb"};
    for (unsigned i = 0; i < sizeof caps / sizeof *caps; i++)
        CHECK(AI(caps[i], args) && g_rc.status != INK_RC_DONE, "AI cannot read an old photo through %s", caps[i]);
    CHECK(AI("app.open", "{\"id\":\"camera\"}") && g_rc.status == INK_RC_REJECTED, "AI cannot open private pages while locked, including a child's lock");
    CHECK(AI("camera.photo", NULL) && g_rc.status == INK_RC_REJECTED, "locked AI cannot trigger camera work");
    CHECK(AI("camera.capture", NULL) && g_rc.status == INK_RC_REJECTED, "locked AI cannot access the retained camera frame");
    CHECK(BY_HAND("app.open", "{\"id\":\"gallery\"}") && g_rc.status == INK_RC_REJECTED, "person cannot open Gallery from restricted capture");
    CHECK(BY_HAND("app.open", "{\"id\":\"files\"}") && g_rc.status == INK_RC_REJECTED, "person cannot open Files from restricted capture");
    CHECK(BY_HAND("shell.launch", "{\"id\":\"settings\"}") && g_rc.status == INK_RC_REJECTED, "other launch routes remain locked");
    CHECK(BY_HAND("file.thumb", args) && g_rc.status == INK_RC_REJECTED, "person cannot request an old thumbnail by known id");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "initial member space opens"); test_start_home(); quiet();
    ok("app.open", "{\"id\":\"camera\"}"); ok("camera.photo", NULL);
    CHECK(count("/me") == 1, "preexisting adult photo exists");
    uint32_t cursor = 0; ink_fs_entry_t e;
    CHECK(ink_fs_index("/me", INK_FS_IMAGE, &cursor, &e) == 1, "old private photo has an id"); old_id = e.id;
    ok("system.home", NULL);
    ink_power_lock(); CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 3000), "seed session locks"); quiet();
    press("app.open"); CHECK(ink_camera_restricted(), "physical locked camera entry opens restricted view");
    ok("camera.photo", NULL); key(HAL_KEY_HOME); quiet();
    CHECK(!ink_camera_restricted() && ink_power_state() == INK_PWR_LOCKED, "seed session HOME returns to locked screen");
    key(HAL_KEY_POWER); quiet();
    bool gate = !strcmp(phase, "gate"), kid = !strcmp(phase, "kid");
    if (gate) {
        CHECK(ink_member_set_pin("owner", "", "1111", told, NULL) == 0 && WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_OK, "set synthetic adult PIN");
        CHECK(setting_by_hand("member.idle_return_s", 30), "use the existing minimum lock grace"); quiet();
    }
    if (kid) CHECK(ink_member_switch("kid") == 0, "child is the active user before locking");
    ink_power_lock(); CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 3000), "test session locks"); quiet();
    if (gate) {
        CHECK(WAIT_FOR(!ink_member_space_open(), 35000), "adult grace expires without unlocking");
        key(HAL_KEY_POWER); quiet(); CHECK(!strcmp(ink_ui_top_name(), "member_gate"), "member gate appears");
        capture("member-entry"); press("app.open"); CHECK(ink_camera_restricted(), "member gate has a working camera entry");
        press("shell.back"); quiet(); key(HAL_KEY_POWER); quiet(); on_ui(open_pin); quiet();
        CHECK(!strcmp(ink_ui_top_name(), "member_gate"), "PIN entry remains a locked member surface");
        capture("pin-entry");
    }
    capture("lock-entry"); press("app.open");
    CHECK(ink_camera_restricted() && shots.count == 0, "new session excludes every earlier locked photo");
    CHECK(BY_HAND("camera.review", NULL) && g_rc.status == INK_RC_FAILED, "empty session cannot review an earlier photo"); quiet();
    ok("camera.preview", NULL);
    CHECK(WAIT_FOR((on_ui(frame), frame_ready), 10000), "initial viewfinder has a captured frame");
    on_ui(inspect); capture("restricted"); blocked();
    press("camera.photo"); CHECK(WAIT_FOR(shots.count == 1, 10000), "touch shutter saves the first session photo"); quiet();
    press("camera.photo"); CHECK(WAIT_FOR(shots.count == 2, 10000), "touch shutter saves the second session photo"); quiet();
    CHECK(count(LOCK_PHOTOS) == 3, "locked captures are stored outside every member");
    CHECK(count("/me") == (kid ? 0u : 1u), "locked capture never enters the active child's or adult's private album");
    CHECK(shots.ids[0] && shots.ids[1] && shots.ids[0] != shots.ids[1] && shots.ids[0] != old_id && shots.ids[1] != old_id, "only two new durable photo ids are reviewable");
    on_ui(inspect); capture("confirmation");
    press("camera.review"); CHECK(WAIT_FOR(shots.reviewing && shots.at == 0, 10000), "review cycles to first photo of this session"); quiet();
    press("camera.review"); CHECK(WAIT_FOR(shots.reviewing && shots.at == 1, 10000), "review cycles to second photo of this session"); quiet();
    press("camera.review"); CHECK(WAIT_FOR(shots.at == 0 && shots.count == 2, 10000), "review wraps within this session"); quiet();
    press("camera.preview"); CHECK(WAIT_FOR(!shots.reviewing && shots.count == 2, 10000), "tapping the picture resumes the viewfinder"); quiet();
    on_ui(inspect); blocked();
    key(HAL_KEY_HOME); quiet(); CHECK(!ink_camera_restricted() && ink_power_state() == INK_PWR_LOCKED, "HOME closes restricted capture and keeps the device locked");
    if (gate) CHECK(!ink_member_space_open(), "HOME does not authenticate the adult");
    key(HAL_KEY_POWER); quiet();
    if (gate) {
        CHECK(!ink_member_space_open(), "wake still requires a PIN"); on_ui(open_pin); on_ui(enter_pin);
        CHECK(WAIT_FOR(ink_member_space_open(), 6000), "real device PIN UI authenticates the adult"); quiet();
    }
    CHECK(ink_member_space_open(), "normal member space is open after wake/authentication");
    ok("app.open", "{\"id\":\"gallery\"}");
    CHECK(WAIT_FOR((on_ui(read_screen), strstr(g_snap, kid ? "\"total\":3" : "\"total\":4")), 10000), "unlocked Gallery contains the public captures: %.150s", g_snap);
    if (kid) CHECK(!strstr(g_snap, "1111"), "child gallery has no adult credential");
    if (kid) {
        ink_power_lock(); CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 3000), "lock before interrupted child capture"); quiet();
        press("app.open"); ok("camera.preview", NULL);
        CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "background index writes settle before interrupted capture");
        ink_call_opts_t o = {.src = INK_SRC_TOUCH, .reply_exec = INK_EXEC_DIRECT, .done = saved};
        photo_done = 0;
        CHECK(ink_cap_call("camera.photo", NULL, &o) == 0 && WAIT_FOR(__atomic_load_n(&taking, __ATOMIC_ACQUIRE), 1000), "shutter is actively capturing before HOME");
        CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "person's HOME capability closes an in-flight capture");
        CHECK(WAIT_FOR(__atomic_load_n(&photo_done, __ATOMIC_ACQUIRE), 10000) && photo_status == INK_RC_DONE, "accepted capture finishes after HOME"); quiet();
        CHECK(count(LOCK_PHOTOS) == 4 && count("/me") == 0, "in-flight locked capture stays in public storage after HOME");
        CHECK(!ink_camera_restricted() && ink_power_state() == INK_PWR_LOCKED, "capture completion cannot reopen or unlock the camera: restricted=%d power=%s", ink_camera_restricted(), power_state());
    }
    CHECK(ink_cap_find("camera.review") != NULL, "session review is registered in the atomic capability table");
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "index and member data are persisted");
    ink_probe_count("test.finished");
}
static void options(void *arg) { (void)arg; const char *s = getenv("INK_LOCK_PHASE"); snprintf(phase, sizeof phase, "%s", s ? s : "lock"); }
void ink_main(void) {
    vtk_host_call(options, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "lock-camera-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start test driver");
}
