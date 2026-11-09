/* Five real settings pages, shared capabilities, persistence and member isolation. */
#include "ink/sys/file_picker.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>
#include "settings_b_fixture.h"

static const char *const PAGES[] = {"SetMembers", "SetKid", "Wallpaper", "Records", "AiMemory"};
static const char *const CAPS[] = {"shell.family", "shell.kid_settings", "shell.wallpaper", "shell.records", "shell.ai_memory"};
static bool restarting, captured;
static const char *opening, *label, *scene;
static char snapshot[8192], added[16];
static int x, y;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static void read_snapshot(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "page has an AI snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "snapshot fits the real AI context");
}
static void find_label(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), label)) {
        lv_area_t a; lv_obj_update_layout(o); lv_obj_get_coords(o, &a); x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, i));
}
static void find(void) { x = y = -1; find_label(lv_screen_active()); }
static void press(const char *text) {
    label = text; on_ui(find); CHECK(x >= 0 && y >= 0, "visible control %s", text);
    if (x >= 0 && y >= 0) tap(x, y);
    quiet();
}
static void save_snapshot(void *arg) {
    char path[600]; int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, (const char *)arg);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb");
    if (!f || fputs(snapshot, f) == EOF || fclose(f)) abort();
}
static void take_frame(void *arg) {
    ink_probe_count("test.snapshot.settled");
    char metric[96]; snprintf(metric, sizeof metric, "test.snapshot.%s", scene ? scene : (const char *)arg); ink_probe_count(metric);
    sim_display_snapshot(arg); captured = true;
}
static void capture(const char *name) {
    quiet(); on_ui(read_snapshot); vtk_host_call(save_snapshot, (void *)name); captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), take_frame, (void *)name);
    CHECK(WAIT_FOR(captured, 1000), "capture settled page %s", name);
}
static void confirm(void) {
    ink_confirm_view_t v;
    CHECK(ink_confirm_list(&v, 1) == 1, "destructive or identity operation waits for a person");
    CHECK(ink_confirm_decide(v.token, true, INK_SRC_TOUCH) == 0, "person confirms through the existing policy");
    quiet();
}
static const ink_member_t *member(const char *id) {
    for (int i = 0; ink_member_at(i); i++) if (!strcmp(ink_member_at(i)->id, id)) return ink_member_at(i);
    return NULL;
}
static bool renamed(void) { const ink_member_t *m = member("kid"); return m && !strcmp(m->name, "合成孩子"); }
static void direct_open(void) { ink_shell_open_page(opening); }
static void hidden(void) { CHECK(!strcmp(ink_ui_page_name(), "home"), "child direct page guard denies all five pages"); }
static void type_pin(void) { ink_text_commit_utf8("2468"); }
static void run(void *arg) {
    (void)arg;
    if (restarting && ink_member_has_pin("owner")) {
        CHECK(BY_HAND("member.unlock", "{\"id\":\"owner\",\"pin\":\"2468\"}") && g_rc.status == INK_RC_DONE, "saved PIN authenticates after restart");
    }
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space is open"); test_start_home(); quiet();
    if (scene) {
        for (int i = 0; i < 5; i++) if (!strcmp(scene, PAGES[i])) {
            CHECK(AI(CAPS[i], NULL) && g_rc.status == INK_RC_DONE, "open the requested production scene"); quiet();
            CHECK(!strcmp(ink_ui_page_name(), scene), "the screenshot is the requested page"); capture("scene");
            ink_probe_count("test.finished"); return;
        }
        CHECK(false, "unknown requested scene"); return;
    }
    if (!restarting) {
        for (int i = 0; i < 5; i++) {
            CHECK(AI(CAPS[i], NULL) && g_rc.status == INK_RC_DONE, "AI opens %s", PAGES[i]); quiet();
            CHECK(!strcmp(ink_ui_page_name(), PAGES[i]), "real activity opens"); capture(PAGES[i]);
            CHECK(strstr(snapshot, "\"fields\"") && strstr(snapshot, "\"actions\""), "F1 exposes fields and actions");
            CHECK(BY_HAND("system.home", NULL), "close settings page"); quiet();
        }
        CHECK(AI("shell.family", NULL), "open member management"); quiet();
        CHECK(AI("family.edit", "{\"id\":\"kid\"}"), "select the real child"); quiet();
        CHECK(AI("family.focus", NULL), "open F2 name input"); quiet();
        CHECK(AI("text.insert", "{\"text\":\"合成孩子\"}"), "AI fills the same name input");
        press("保存称呼"); CHECK(WAIT_FOR(renamed(), 10000), "touch and AI share member.rename and its backend");
        CHECK(BY_HAND("system.home", NULL), "leave member editor"); quiet();
        CHECK(AI("shell.family", NULL), "reopen member list"); quiet(); press("加一位家人");
        CHECK(AI("family.focus", NULL), "focus addition name"); quiet();
        CHECK(AI("text.insert", "{\"text\":\"合成访客\"}"), "enter synthetic new name");
        CHECK(AI("family.role", NULL), "select an adult through the same role control"); press("保存称呼");
        confirm(); CHECK(WAIT_FOR(member("owner") && ink_member_at(2), 10000), "addition reaches durable membership");
        for (int i = 0; ink_member_at(i); i++) if (!strcmp(ink_member_at(i)->name, "合成访客")) {
            snprintf(added, sizeof added, "%s", ink_member_at(i)->id); CHECK(ink_member_at(i)->role == INK_ROLE_ADULT, "new role comes from the form");
        }
        CHECK(added[0], "the synthetic visitor exists");
        CHECK(BY_HAND("system.home", NULL), "leave addition"); quiet(); CHECK(AI("shell.family", NULL), "reopen family"); quiet();
        char args[96]; snprintf(args, sizeof args, "{\"id\":\"%s\"}", added);
        CHECK(AI("family.edit", args), "edit the added visitor"); quiet(); press("移除这位家人");
        CHECK(ink_member_exists(added), "member is not removed before human confirmation"); confirm();
        CHECK(WAIT_FOR(!ink_member_exists(added), 10000), "confirmed removal completes");
        CHECK(AI("shell.kid_settings", NULL), "open child controls"); quiet();
        press("孩子的音量上限"); confirm(); CHECK(WAIT_FOR(ink_setting_get("audio.child_max") == 70, 10000), "touch uses the existing child setting");
        CHECK(AI("settings.set", "{\"id\":\"audio.child_max\",\"value\":90}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI uses the same settings policy");
        confirm(); CHECK(WAIT_FOR(ink_setting_get("audio.child_max") == 90, 10000), "AI change reaches backend");
        CHECK(ink_fs_write_atomic("/family/SYNTH_WALL.png", SYNTH_PNG, sizeof SYNTH_PNG) == 0, "store a synthetic PNG fixture");
        uint32_t cursor = 0, image = 0; ink_fs_entry_t entry;
        while (ink_fs_index("/family", INK_FS_IMAGE, &cursor, &entry) == 1) if (strstr(entry.path, "SYNTH_WALL")) image = entry.id;
        CHECK(image != 0, "image has a stable service id");
        CHECK(AI("shell.wallpaper", NULL), "open wallpaper page"); quiet(); press("从相册选");
        bool picker_ready = false;
        for (int i = 0; i < 200; i++) { on_ui(read_snapshot); if (strstr(snapshot, "\"status\":\"\"")) { picker_ready = true; break; } delay_ms(20); }
        CHECK(picker_ready && strstr(snapshot, "SYNTH_WALL"), "F9 shows the real indexed image");
        snprintf(args, sizeof args, "{\"id\":%u}", image);
        CHECK(AI("file_picker.select", args) && g_rc.status == INK_RC_DONE, "AI selects the same image as touch");
        CHECK(WAIT_FOR(ink_wallpaper_ready(), 20000), "image decode and dithering reach the UI"); quiet(); capture("WallpaperSelected");
        CHECK(BY_HAND("system.home", NULL), "show wallpaper on home"); quiet(); capture("HomeWallpaper");
        CHECK(BY_HAND("shell.lock", NULL), "lock with the chosen wallpaper"); quiet(); capture("LockWallpaper");
        key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_member_space_open(), 10000), "wake without changing identity"); quiet();
        CHECK(AI("shell.family", NULL), "open PIN settings"); quiet(); CHECK(BY_HAND("family.pin", NULL), "enter real PIN form"); quiet();
        CHECK(BY_HAND("family.focus", NULL), "focus the private new PIN"); quiet(); on_ui(type_pin); on_ui(read_snapshot);
        CHECK(!strstr(snapshot, "2468"), "PIN is masked in AI snapshots");
        CHECK(AI("text.insert", "{\"text\":\"9999\"}") && g_rc.status == INK_RC_REJECTED, "AI cannot fill a password");
        CHECK(AI("family.pin_save", NULL) && g_rc.status == INK_RC_INVALID, "AI cannot submit the password");
        CHECK(BY_HAND("family.pin_save", NULL), "person submits the PIN through member.set_pin");
        CHECK(WAIT_FOR(ink_member_has_pin("owner"), 10000), "PIN reaches the member backend");
        CHECK(BY_HAND("system.home", NULL), "leave the private form"); quiet();
        CHECK(WAIT_FOR(sim_metric_value("settings.saved") > 0 && ink_storage_pending() == 0, 10000), "all settings reach persistent storage");
        CHECK(ink_member_switch("kid") == 0, "enter the child space"); quiet();
        for (int i = 0; i < 5; i++) {
            CHECK(AI(CAPS[i], NULL) && g_rc.status == INK_RC_REJECTED, "child AI cannot open %s", PAGES[i]);
            opening = PAGES[i]; on_ui(direct_open); quiet(); on_ui(hidden); on_ui(ink_ui_home); quiet();
        }
        CHECK(AI("member.rename", "{\"id\":\"owner\",\"name\":\"不该生效\"}") && g_rc.status == INK_RC_REJECTED, "child cannot rename an adult");
        CHECK(AI("settings.set", "{\"id\":\"display.wallpaper_file\",\"value\":0}") && g_rc.status == INK_RC_REJECTED, "child cannot change the adult wallpaper setting");
    } else {
        CHECK(renamed(), "renamed member survives a fresh process");
        CHECK(ink_setting_get("audio.child_max") == 90, "child setting survives a fresh process");
        CHECK(ink_setting_get("display.wallpaper_file") > 0, "chosen wallpaper id survives a fresh process");
        CHECK(ink_member_at(2) == NULL, "deleted member stays deleted after restart");
        CHECK(WAIT_FOR(ink_wallpaper_ready(), 20000), "saved source image is decoded after restart");
        CHECK(AI("shell.records", NULL), "read the persisted AI activity"); quiet(); capture("RecordsRestart");
        CHECK(!strstr(snapshot, "还没有记录"), "AI activity is real persisted backend data");
    }
    CHECK(WAIT_FOR(ink_storage_pending() == 0, 10000), "pending writes finish"); ink_probe_count("test.finished");
}
void ink_main(void) {
    restarting = getenv("INK_SETTINGS_B_RESTART") != NULL; scene = getenv("INK_SETTINGS_B_SCENE"); product_main();
    osal_task_cfg_t cfg = {.name = "settings-b-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start settings scenario driver");
}
