/* Real settings, lock events, member isolation and renderer coverage. */
#include "ink/ui_shell.h"
#include "ink/sys/time.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include "settings_b_fixture.h"
#include "lock_faces_fixture.h"
#include <stdlib.h>

const char *ink_lunar_solar_term(int year, int month, int day);
static char control[16];
static void host_control(void *arg) {
    (void)arg; const char *p = getenv("INK_LOCK_FACES_CASE"); snprintf(control, sizeof control, "%.15s", p ? p : "");
}
#ifndef INK_TEST_LOCK_SOURCE
#define INK_TEST_LOCK_SOURCE "../system/ui/lock_mark.c"
#endif
#include INK_TEST_LOCK_SOURCE

static char snapshot[8192], frame_name[64];
static int snapshot_len, context_len;
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
    snapshot_len = ink_ui_snapshot(snapshot, sizeof snapshot, NULL);
    context_len = ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL);
}
static void check_page(void) {
    ink_card_t card = {0};
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *app = ink_app_at(i);
        if (!strcmp(app->id, "reader") && app->card) app->card(0, &card);
    }
    CHECK(card.value == 5 && card.has_value, "the real reader restores the saved fifth page");
}
static bool has(const char *text) {
    uint64_t end = osal_now_us() + 5000000;
    do {
        on_ui(inspect);
        if (snapshot_len > 0 && context_len > 0) break;
        delay_ms(20); /* A concurrent authentication change invalidates an in-flight snapshot. */
    } while (osal_now_us() < end);
    CHECK(snapshot_len > 0, "lock snapshot fits");
    CHECK(context_len > 0, "visible lock fields fit the actual AI context");
    return strstr(snapshot, text) != NULL;
}
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
static void locate(void) { tx = ty = -1; find(g_mark ? g_mark : lv_screen_active()); }
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
    setting("display.lock_style", 2); on_ui(render); expect_alpha = alpha;
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
                setting("display.lock_style", face); on_ui(render); quiet();
                if (face == 1 && background) { on_ui(image_pixels); CHECK(pixels_ready, "legacy minute clock overlays the selected PNG"); }
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
static void picker(void) {
    for (int size = 1; size <= 3; size += 2) {
        setting("display.font_size", size);
        CHECK(BY_HAND("shell.wallpaper", NULL) && g_rc.status == INK_RC_DONE, "open real wallpaper settings"); quiet();
        touch("锁屏时显示");
        CHECK(!strcmp(ink_ui_page_name(), "LockFaces"), "settings row opens the face picker without cycling");
        char name[64]; snprintf(name, sizeof name, "picker-size-%d", size); frame(name);
        for (int face = 0; face < INK_LOCK_FACE_COUNT; face++) {
            touch(ink_lock_face_choices[face]);
            CHECK(ink_setting_get("display.lock_style") == face && has("已选择"), "tap selects face %d", face);
            CHECK(BY_HAND("shell.lock", NULL), "lock after selecting face %d", face); quiet();
            CHECK(has(ink_lock_face_choices[face]), "real lock shows selected face %d", face);
            key(HAL_KEY_POWER); quiet();
            int other = (face + 1) % INK_LOCK_FACE_COUNT;
            char args[96]; snprintf(args, sizeof args, "{\"id\":\"display.lock_style\",\"value\":%d}", other);
            CHECK(AI("settings.set", args) && g_rc.status == INK_RC_DONE, "AI selects face %d with settings.set", other); quiet();
            CHECK(BY_HAND("shell.lock", NULL), "lock after AI selection"); quiet();
            CHECK(has(ink_lock_face_choices[other]), "AI and touch share face definitions");
            key(HAL_KEY_POWER); quiet();
        }
        touch("锁屏显示待办"); CHECK(ink_setting_get("display.lock_todo") == (size == 1), "picker toggles to-dos");
        touch("锁屏继续读"); CHECK(ink_setting_get("display.lock_reading") == (size == 1), "picker toggles reading");
    }
    CHECK(BY_HAND("system.home", NULL), "leave the picker"); quiet();
}
static void calendar_dates(void) {
    setting("display.lock_style", INK_LOCK_CALENDAR); date(2023, 3, 21, 23, 59); quiet();
    CHECK(has("二月三十"), "calendar uses existing lunar conversion before leap month"); on_ui(unchanged);
    date(2023, 3, 22, 0, 0);
    CHECK(has("闰二月初一") && strstr(snapshot, "22:闰二月（今天）"), "cross-day rebuild shows leap month name on first day"); frame("leap-month");
    on_ui(unchanged);
    date(2026, 11, 22, 0, 0);
    CHECK(has("小雪"), "same minute on a new date updates through time.changed");
    date(2026, 11, 23, 0, 0);
    CHECK(!has("小雪"), "ordinary dates do not invent a solar term");
}
static void solar_terms(void) {
    /* Hong Kong Observatory's 2026 calendar, not the compressed implementation table. */
    static const int days[] = {5,20,4,18,5,20,5,20,5,21,5,21,7,23,7,23,7,23,8,23,7,22,7,22};
    static const char *const names[] = {"小寒","大寒","立春","雨水","惊蛰","春分","清明","谷雨","立夏","小满","芒种","夏至",
        "小暑","大暑","立秋","处暑","白露","秋分","寒露","霜降","立冬","小雪","大雪","冬至"};
    _Static_assert(sizeof days / sizeof *days == sizeof names / sizeof *names, "reference terms match");
    for (size_t i = 0; i < sizeof days / sizeof *days; i++) {
        const char *term = ink_lunar_solar_term(2026, (int)i / 2 + 1, days[i]);
        CHECK(term && !strcmp(term, names[i]), "official 2026 solar term %zu matches", i);
        CHECK(!ink_lunar_solar_term(2026, (int)i / 2 + 1, days[i] + 1), "no term on the following day");
    }
    CHECK(!ink_lunar_solar_term(1900, 1, 1) && !ink_lunar_solar_term(2101, 1, 1), "solar table bounds are explicit");
    CHECK(!ink_lunar_solar_term(2026, 0, 1) && !ink_lunar_solar_term(2026, 13, 1), "invalid months have no term");
}
static void clock_bounds(void) {
    lv_obj_t *clock = lv_obj_get_child(lv_obj_get_child(g_mark, 0), 0);
    lv_area_t a; lv_obj_update_layout(clock); lv_obj_get_coords(clock, &a);
    ink_probe_value("test.minute.x1", a.x1); ink_probe_value("test.minute.y1", a.y1);
    ink_probe_value("test.minute.x2", a.x2); ink_probe_value("test.minute.y2", a.y2);
}
static char clock_text[24], old_clock_text[24];
static void read_clock(void) { snprintf(clock_text, sizeof clock_text, "%s", lv_label_get_text(lv_obj_get_child(lv_obj_get_child(g_mark, 0), 0))); }
static void minute_pixels(void) {
    setting("display.lock_style", INK_LOCK_SIMPLE); date(2026, 11, 11, 8, 30); on_ui(render);
    frame("minute-before"); on_ui(clock_bounds); on_ui(read_clock); snprintf(old_clock_text, sizeof old_clock_text, "%s", clock_text);
    ink_time_set(ink_time_now_s() + 60, false); quiet(); on_ui(read_clock);
    CHECK(strcmp(clock_text, old_clock_text), "minute event updates clock text"); frame("minute-after");
    on_ui(unchanged); frame("minute-same");
}
static void automatic_minute(void) {
    setting("display.lock_style", INK_LOCK_SIMPLE); date(2026, 11, 22, 8, 30); on_ui(render); quiet();
    on_ui(read_clock); snprintf(old_clock_text, sizeof old_clock_text, "%s", clock_text);
    delay_ms(65000); /* Advance virtual time across a real service deadline. */
    on_ui(read_clock);
    CHECK(strcmp(clock_text, old_clock_text), "locked minute advances automatically through the time service");
}
static void english(void) {
    setting("system.language", 1); setting("display.lock_style", INK_LOCK_CALENDAR);
    date(2026, 11, 22, 0, 0); on_ui(render); quiet();
    CHECK(has("Light snow") && strstr(snapshot, "Lunar"), "calendar translates lunar dates and solar terms"); frame("calendar-en");
    key(HAL_KEY_POWER); quiet();
    CHECK(AI("shell.lock_faces", NULL) && g_rc.status == INK_RC_DONE, "AI can open the same picker"); quiet();
    CHECK(has("Simple Clock") && strstr(snapshot, "Show to-dos"), "picker translates faces and widgets"); frame("picker-en");
}
static void privacy_and_reading(void) {
    /* Page five was saved at the first text size; at another size the reader keeps the same first sentence on top
     * and numbers the page by the new pagination, so resume at the size it was read in. */
    setting("display.font_size", 1);
    setting("display.lock_style", INK_LOCK_STANDBY); on_ui(render); quiet();
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
    ink_power_hold(INK_NEED_AWAKE); /* Keep the UI test driver runnable between locked-page interactions. */
    CHECK(AI("settings.set", "{\"id\":\"display.lock_style\",\"value\":3}") && g_rc.status == INK_RC_DONE,
          "live settings accept the new enum through AI");
    CHECK(AI("settings.set", "{\"id\":\"display.lock_todo\",\"value\":1}") && g_rc.status == INK_RC_DONE, "AI uses the existing settings capability for widget selection");
    setting("display.lock_todo", 0);
    if (!strcmp(control, "picker")) { picker(); ink_probe_count("test.finished"); return; }
    date(2026, 11, 11, 8, 30); seed(); CHECK(BY_HAND("shell.lock", NULL), "lock the real device model"); quiet();
    if (control[0]) {
        setting("display.lock_todo", 1); setting("display.lock_reading", 1);
        if (!strcmp(control, "minute")) { setting("display.lock_style", INK_LOCK_SIMPLE); on_ui(render); on_ui(unchanged); }
        else if (!strcmp(control, "day") || !strcmp(control, "solar") || !strcmp(control, "events")) calendar_dates();
        else if (!strcmp(control, "auto")) automatic_minute();
        else if (!strcmp(control, "tick")) minute_pixels();
        else if (!strcmp(control, "english")) english();
        else if (strcmp(control, "enum")) privacy_and_reading();
        ink_probe_count("test.finished"); return;
    }
    matrix(); calendar_dates(); solar_terms(); privacy_and_reading(); ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_control, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "lock-faces-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start lock renderer fixture");
}
