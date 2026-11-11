/* Renderer fixture: new enum values remain rejected by the out-of-scope settings table. */
#include "ink/ui_shell.h"
#include "ink/sys/time.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include "settings_b_fixture.h"
#include "lock_faces_fixture.h"
#include <stdlib.h>

static int style = -1;
static int64_t renderer_setting(const char *id) { return style >= 0 && !strcmp(id, "display.lock_style") ? style : ink_setting_get(id); }
#define ink_setting_get renderer_setting
#ifndef INK_TEST_LOCK_SOURCE
#define INK_TEST_LOCK_SOURCE "../system/ui/lock_mark.c"
#endif
#include INK_TEST_LOCK_SOURCE
#undef ink_setting_get

static char snapshot[8192], frame_name[64];
static const char *wanted;
static bool captured;
static int tx, ty;
static bool expect_alpha, pixels_ready;
static void image_pixels(void) {
    pixels_ready = false;
    for (uint32_t i = 0; g_mark && i < lv_obj_get_child_count(g_mark); i++) {
        lv_obj_t *o = lv_obj_get_child(g_mark, (int)i);
        if (!lv_obj_check_type(o, &lv_image_class)) continue;
        const lv_image_dsc_t *d = lv_image_get_src(o);
        if (d && d->header.w == 64 && d->header.h == 96 && d->data)
            pixels_ready = expect_alpha ? d->data[0] == 255 && d->data[70 * 64] > d->data[70 * 64 + 50] : d->data[0] == 0;
    }
}
static bool decoded_pixels(void) { on_ui(image_pixels); return pixels_ready; }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 15000), "lock display settles"); }
static void inspect(void) {
    CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "lock snapshot fits");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "visible lock fields fit the actual AI context");
}
static void check_page(void) {
    ink_card_t card = {0};
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *app = ink_app_at(i);
        if (!strcmp(app->id, "reader") && app->card) app->card(0, &card);
    }
    CHECK(card.value == 5 && card.has_value, "the real reader restores the saved fifth page");
}
static bool has(const char *text) { on_ui(inspect); return strstr(snapshot, text) != NULL; }
static void render(void) { hide(); show(); }
static void capture(void *arg) { sim_display_snapshot(arg); captured = true; }
static void frame(const char *name) {
    quiet(); snprintf(frame_name, sizeof frame_name, "%s", name); captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), capture, frame_name);
    CHECK(WAIT_FOR(captured, 1000), "capture %s", name);
}
static void setting(const char *id, int value) {
    ink_receipt_t rc;
    CHECK(ink_setting_set(id, value, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "fixture setting %s passes the existing setting policy", id); quiet();
}
static void date(int year, int month, int day, int hour, int minute) {
    ink_tm_t t = {.year = year, .month = month, .day = day, .hour = hour, .min = minute};
    ink_time_set(ink_time_from_local(&t), false); quiet();
}
static void unchanged(void) {
    on_minute(NULL, 0, "time.minute");
    uint32_t before = g_revision; on_minute(NULL, 0, "time.minute");
    CHECK(g_revision == before, "same minute does not refresh or advance the face revision");
}
static void find(lv_obj_t *o) {
    if (lv_obj_check_type(o, &lv_label_class) && strstr(lv_label_get_text(o), wanted)) {
        lv_area_t a; lv_obj_get_coords(o, &a); tx = (a.x1 + a.x2) / 2; ty = (a.y1 + a.y2) / 2;
        CHECK(a.y2 < ink_ui_px(INK_CANVAS_H - INK_MARGIN), "widget stays above the footer");
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find(lv_obj_get_child(o, (int)i));
}
static void locate(void) { tx = ty = -1; find(g_mark); }
static void touch(const char *text) {
    wanted = text; on_ui(locate); CHECK(tx >= 0 && ty >= 0, "real reading target exists");
    if (tx >= 0 && ty >= 0) tap(tx, ty);
    quiet();
}
static void seed(void) {
    const ink_db_def_t *def = ink_db_find_def("memo.notes"); ink_db_t *db = ink_db_open(def, NULL);
    const char *note = "{\"body\":\"合成已完成\\n合成成人私密待办\",\"checklist\":true,\"checked\":1}";
    ink_db_write_t w = {.data = note, .len = strlen(note), .src = INK_SRC_TOUCH};
    CHECK(db && !ink_db_put(db, &w, NULL), "seed a synthetic checklist through the memo collection");
    ink_db_close(db);
    CHECK(BY_HAND("reader.open_book", "{\"title\":\"测试长书\"}") && g_rc.status == INK_RC_DONE, "open synthetic long book"); quiet();
    CHECK(BY_HAND("reader.goto_page", "{\"page\":5}") && g_rc.status == INK_RC_DONE, "remember page five"); quiet();
    CHECK(BY_HAND("system.home", NULL), "leave the book"); quiet();
}
static void wallpaper(bool alpha) {
    CHECK(ink_fs_write_atomic(alpha ? "/family/SYNTH_LOCK_ALPHA.png" : "/family/SYNTH_LOCK.png", alpha ? SYNTH_ALPHA : SYNTH_PNG,
          alpha ? sizeof SYNTH_ALPHA : sizeof SYNTH_PNG) == 0, "store synthetic wallpaper");
    uint32_t cursor = 0, id = 0; ink_fs_entry_t e;
    while (ink_fs_index("/family", INK_FS_IMAGE, &cursor, &e) == 1) if (!strcmp(e.path, alpha ? "/family/SYNTH_LOCK_ALPHA.png" : "/family/SYNTH_LOCK.png")) id = e.id;
    CHECK(id != 0, "wallpaper passes the existing storage index"); setting("display.wallpaper_file", (int)id);
    CHECK(WAIT_FOR(ink_wallpaper_ready(), 15000), "real wallpaper decode completes");
    style = 2; on_ui(render); expect_alpha = alpha;
    CHECK(WAIT_FOR(decoded_pixels(), 15000), "opaque and transparent PNG use their own decoded pixels");
}
static void matrix(void) {
    setting("display.lock_todo", 1); setting("display.lock_reading", 1);
    for (int size = 1; size <= 3; size += 2) {
        setting("display.font_size", size);
        for (int background = 0; background < 3; background++) {
            if (background) wallpaper(background == 2);
            setting("display.lock_background", background != 0);
            for (int face = 0; face < INK_LOCK_FACE_COUNT; face++) {
                style = face; on_ui(render); quiet();
                if (face >= INK_LOCK_STANDBY) CHECK(WAIT_FOR(has("合成成人私密待办"), 15000), "current owner sees unchecked memo");
                on_ui(unchanged);
                char name[64]; snprintf(name, sizeof name, "face-%d-size-%d-bg-%d", face, size, background); frame(name);
                if (face >= INK_LOCK_STANDBY) {
                    CHECK(has("第 5 页") && !strstr(snapshot, "合成已完成"), "same reading card and unfinished checklist");
                    wanted = "合成成人私密待办"; on_ui(locate);
                }
            }
        }
    }
}
static void calendar_dates(void) {
    style = INK_LOCK_CALENDAR; date(2023, 3, 21, 23, 59); on_ui(render); quiet();
    CHECK(has("二月三十"), "calendar uses existing lunar conversion before leap month"); on_ui(unchanged);
    date(2023, 3, 22, 0, 0);
    CHECK(WAIT_FOR(has("闰二月初一"), 5000) && strstr(snapshot, "22:闰二月（今天）"), "cross-day rebuild shows leap month name on first day"); frame("leap-month");
    on_ui(unchanged);
}
static void privacy_and_reading(void) {
    style = INK_LOCK_STANDBY; on_ui(render); quiet();
    CHECK(WAIT_FOR(has("第 5 页"), 5000), "adult reading widget is ready");
    CHECK(ink_member_switch("kid") == 0, "switch to child while locked"); quiet();
    wanted = "合成成人私密待办"; on_ui(locate); CHECK(tx < 0, "child pixels contain no adult memo label");
    CHECK(!has("合成成人私密待办") && !strstr(snapshot, "测试长书") && !strstr(snapshot, "第 5 页"), "child lock and AI snapshot have no adult memo or reading data"); frame("child");
    CHECK(ink_member_switch("owner") == 0, "return to adult"); quiet();
    key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_power_state() != INK_PWR_LOCKED, 5000), "wake before setting the PIN"); quiet();
    CHECK(BY_HAND("member.set_pin", "{\"id\":\"owner\",\"new_pin\":\"2468\"}") && g_rc.status == INK_RC_DONE,
          "person sets synthetic PIN: %s %.90s", ink_rc_name(g_rc.status), g_rc.detail);
    CHECK(BY_HAND("shell.lock", NULL), "start a fresh adult lock grace"); quiet();
    CHECK(WAIT_FOR(!ink_member_space_open(), 150000), "wait for the actual adult lock grace to expire"); quiet();
    CHECK(!has("合成成人私密待办") && !strstr(snapshot, "测试长书"), "closed adult space redacts visible widgets and snapshot"); frame("adult-closed");
    touch("解锁后继续读");
    CHECK(ink_power_state() != INK_PWR_LOCKED && !ink_member_space_open() && strcmp(ink_ui_page_name(), "book"), "reading waits at the member gate");
    bool ai_called = AI("member.unlock", "{\"id\":\"owner\",\"pin\":\"2468\"}");
    CHECK((!ai_called || g_rc.status != INK_RC_DONE) && !ink_member_space_open(), "AI cannot authenticate or open the adult space");
    CHECK(BY_HAND("member.unlock", "{\"id\":\"owner\",\"pin\":\"2468\"}") && g_rc.status == INK_RC_DONE, "person authenticates");
    CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "book"), 15000), "unlock resumes the reader"); quiet();
    on_ui(check_page); frame("reading-resumed");
}
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "space opens"); test_start_home();
    CHECK(AI("settings.set", "{\"id\":\"display.lock_style\",\"value\":3}") && g_rc.status == INK_RC_INVALID,
          "documented scope gap: live settings still reject new enum values");
    CHECK(AI("settings.set", "{\"id\":\"display.lock_todo\",\"value\":1}") && g_rc.status == INK_RC_DONE, "AI uses the existing settings capability for widget selection");
    date(2026, 11, 11, 8, 30); seed(); CHECK(BY_HAND("shell.lock", NULL), "lock the real device model"); quiet();
    matrix(); calendar_dates(); privacy_and_reading(); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "lock-faces-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start lock renderer fixture");
}
