/* calendar-3 pages on the real product: month on one screen (default, largest text, child), agenda and search, the day and
 * the almanac, details, the editor and its pages, scopes, undo, settings, import; marks for run.py's refresh checks. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#include "ink/sys/almanac.h"
#include "ink/sys/font.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char snapshot[4096], scene[16], owner[16], wanted_text[96];
static const char *wanted_cap;
static lv_point_t point;
static bool found, fail_almanac;
static int pick;

/* The test replaces the calendar's door to the almanac: a failure on demand (calendar-3 §4). */
bool cal_almanac(int y, int m, int d, ink_almanac_t *out, const char **why);
bool cal_almanac(int y, int m, int d, ink_almanac_t *out, const char **why) {
    if (y < 2000 || y > 2099) { *why = "老黄历只有 2000–2099 年"; return false; }
    if (fail_almanac || !ink_almanac_date(y, m, d, out)) { *why = "这天的老黄历查不到"; return false; }
    return true;
}

static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the page settles"); }
static void ok(const char *cap, const char *args) {
    CHECK(BY_HAND(cap, args) && (g_rc.status == INK_RC_DONE || g_rc.status == INK_RC_QUEUED), "%s done (%s): %.80s", cap, ink_rc_name(g_rc.status), g_rc.detail);
    quiet(); delay_ms(150);
}
static void ai(const char *cap, const char *args) { CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "AI %s (%s): %.80s", cap, ink_rc_name(g_rc.status), g_rc.detail); quiet(); delay_ms(200); }
static void read_snapshot(void) { CHECK(ink_ui_snapshot(snapshot, sizeof snapshot, NULL) > 0, "the page snapshot fits"); }
static void expect(const char *text) { on_ui(read_snapshot); CHECK(strstr(snapshot, text) != NULL, "page says %s (%s): %.200s", text, ink_ui_page_name(), snapshot); }
static void absent(const char *text) { on_ui(read_snapshot); CHECK(!strstr(snapshot, text), "page does not say %s", text); }
static void page(const char *name) { CHECK(!strcmp(ink_ui_page_name(), name), "on %s (%s)", name, ink_ui_page_name()); }
static void locate(lv_obj_t *root) {
    if (!lv_obj_is_visible(root)) return;
    const char *cap = ink_shell_bound_cap(root);
    bool match = wanted_cap ? cap && !strcmp(cap, wanted_cap) : lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), wanted_text);
    if (match && pick-- == 0) { lv_area_t a; lv_obj_get_coords(root, &a); point = (lv_point_t){(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2}; found = true; }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) locate(lv_obj_get_child(root, (int32_t)i));
}
static void locate_ui(void) { found = false; lv_obj_update_layout(lv_screen_active()); locate(lv_screen_active()); }
static void press(const char *cap, int index) {
    wanted_cap = cap; pick = index; on_ui(locate_ui); CHECK(found, "touch target %s #%d", cap, index);
    if (found) tap(point.x, point.y);
    quiet(); delay_ms(150);
}
static void press_text(const char *text) {
    wanted_cap = NULL; snprintf(wanted_text, sizeof wanted_text, "%s", text); pick = 0; on_ui(locate_ui); CHECK(found, "visible text %s", text);
    if (found) tap(point.x, point.y);
    quiet(); delay_ms(150);
}
static void capture_event(void *arg) { sim_display_snapshot(arg); }
static void shot(const char *name) {
    static char names[48][40]; static int n;
    if (n >= 48) return;
    snprintf(names[n], sizeof names[n], "%s-%s", scene, name);
    quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture_event, names[n++]); delay_ms(5);
}
static int64_t at(int y, int mo, int d, int h, int mi) { return ink_time_from_local(&(ink_tm_t){.year = y, .month = mo, .day = d, .hour = h, .min = mi}); }
static uint32_t id_of(const char *title) { ink_event_t e; int c = 0; while (ink_event_next(&c, &e) == 1) if (!strcmp(e.title, title) && !e.parent) return e.id; return 0; }

/* ---- the month grid's geometry, from the real objects ---- */
static int cells, cells_tall, cells_wide, cells_inside, grey_cells, marked[32], clipped;
static void inspect_grid(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    const char *cap = ink_shell_bound_cap(o);
    if (cap && !strcmp(cap, "calendar.select")) {
        lv_area_t a; lv_obj_get_coords(o, &a);
        cells++;
        cells_tall += lv_obj_get_height(o) == ink_ui_px(INK_CONTROL);
        cells_wide += abs(lv_obj_get_width(o) - ink_ui_width() / 7) <= 1;
        cells_inside += a.x1 >= 0 && a.y1 >= 0 && a.x2 < ink_ui_width() && a.y2 < ink_ui_height();
        lv_obj_t *num = lv_obj_get_child(o, 0), *lunar = lv_obj_get_child(o, 1);
        if (num && lv_obj_check_type(num, &lv_label_class)) {
            lv_color_t c = lv_obj_get_style_text_color(num, 0);
            grey_cells += lv_color_eq(c, ink_ui_secondary());
            int day = atoi(lv_label_get_text(num));
            unsigned boxes = 0;
            for (uint32_t i = 2; i < lv_obj_get_child_count(o); i++) boxes += lv_obj_get_height(lv_obj_get_child(o, (int32_t)i)) == ink_ui_px(INK_GAP_S);
            if (!lv_color_eq(c, ink_ui_secondary()) && day > 0 && day < 32 && boxes) marked[day] = (int)boxes;
            lv_point_t size; lv_text_get_size(&size, lv_label_get_text(lunar), lv_obj_get_style_text_font(lunar, 0), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            clipped += size.x > lv_obj_get_width(lunar);
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) inspect_grid(lv_obj_get_child(o, (int32_t)i));
}
static void grid(void) { cells = cells_tall = cells_wide = cells_inside = grey_cells = clipped = 0; memset(marked, 0, sizeof marked); lv_obj_update_layout(lv_screen_active()); inspect_grid(lv_screen_active()); }

/* ---- fonts: every character the day and the almanac pages show (H7) ---- */
static int missing_chars, missing_other, checked, seen;
static void glyphs_of(const char *s, const ink_font_t *font) {
    for (const unsigned char *p = (const unsigned char *)s; *p;) {
        uint32_t cp = *p < 0x80 ? *p : *p < 0xE0 ? (uint32_t)(*p & 0x1F) : *p < 0xF0 ? (uint32_t)(*p & 0x0F) : (uint32_t)(*p & 0x07);
        int n = *p < 0x80 ? 1 : *p < 0xE0 ? 2 : *p < 0xF0 ? 3 : 4;
        for (int k = 1; k < n && p[k]; k++) cp = (cp << 6) | (p[k] & 0x3F);
        p += n;
        if (cp <= ' ') continue;
        checked++;
        if (!ink_font_has(font, cp)) {
            if (cp == 0x78D1) missing_chars++; /* 磑: known, not in GB2312; the main session adds it to CMakeLists.txt:372 */
            else missing_other++;
        }
    }
}
static void font_cover(void) {
    const ink_font_t *font = ink_font_open("fonts/sans-44.inkf"); /* the reading face regular UI text is drawn from */
    ink_almanac_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    for (int32_t day = 10957; a && day < 10957 + 36524; day += 1) { /* 2000-01-01 .. 2099-12-31 */
        int64_t s = (int64_t)day * 86400 + 43200; ink_tm_t t; ink_time_to_zone(1, s, &t);
        if (!ink_almanac_date(t.year, t.month, t.day, a)) continue;
        seen += strstr(a->yi, "磑") || strstr(a->ji, "磑");
        glyphs_of(a->yi, font); glyphs_of(a->ji, font); glyphs_of(a->auspicious, font); glyphs_of(a->inauspicious, font);
        glyphs_of(a->clash, font); glyphs_of(a->sha, font); glyphs_of(a->wealth, font); glyphs_of(a->joy, font); glyphs_of(a->fortune, font);
        glyphs_of(a->guardian, font); glyphs_of(a->guardian_type, font); glyphs_of(a->officer, font);
        glyphs_of(a->year_ganzhi, font); glyphs_of(a->month_ganzhi, font); glyphs_of(a->day_ganzhi, font);
    }
    osal_free(a);
}
static int term_diff;
static char term_first[64];
static void terms(void) { /* lunar.c against Mogo's solar terms, 2000-2099: listed in the report, not a failure */
    ink_almanac_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    for (int32_t day = 10957; a && day < 10957 + 36524; day++) {
        int64_t s = (int64_t)day * 86400 + 43200; ink_tm_t t; ink_time_to_zone(1, s, &t);
        if (!ink_almanac_date(t.year, t.month, t.day, a)) continue;
        const char *ours = ink_lunar_solar_term(t.year, t.month, t.day), *mogo = a->solar_term && a->solar_term[0] ? a->solar_term : NULL;
        if ((ours == NULL) != (mogo == NULL) || (ours && mogo && strcmp(ours, mogo))) {
            if (!term_diff++) snprintf(term_first, sizeof term_first, "%04d-%02d-%02d %s/%s", t.year, t.month, t.day, ours ? ours : "-", mogo ? mogo : "-");
        }
    }
    osal_free(a);
}

static void fixtures(void) {
    ai("reminder.add", "{\"at\":\"2026-11-20 20:00\",\"end\":\"2026-11-22 18:00\",\"title\":\"出差\",\"reminders\":[1440,60]}");
    ai("reminder.add", "{\"at\":\"2026-11-16 10:00\",\"end\":\"2026-11-16 11:00\",\"title\":\"周会\",\"reminders\":[10],\"rule\":{\"freq\":\"weekly\",\"until\":\"2026-12-31\"}}");
    char args[200]; snprintf(args, sizeof args, "{\"id\":%u,\"at\":\"2026-11-24 15:00\",\"end\":\"2026-11-24 16:00\",\"scope\":\"this\",\"occurrence\":\"2026-11-23 10:00\"}", (unsigned)id_of("周会"));
    ai("reminder.update", args);
    ai("reminder.add", "{\"at\":\"2026-11-13\",\"title\":\"整理照片\",\"reminders\":[]}");
    ai("reminder.add", "{\"at\":\"2026-09-25 09:00\",\"title\":\"妈妈生日\",\"repeat\":\"lunar_yearly\"}");
}

static void month(void) {
    ok("calendar.open", NULL); page("Calendar");
    on_ui(grid);
    CHECK(cells == 42 && cells_tall == 42 && cells_wide == 42 && cells_inside == 42, "six weeks on one screen: %d cells, %d 108 tall, %d a seventh wide, %d inside", cells, cells_tall, cells_wide, cells_inside);
    CHECK(marked[20] && marked[21] && marked[22] && marked[16] && marked[13] && !marked[23] && marked[24], "the trip marks 20-22, the weekly meeting 16 and the moved 24, the day off 13");
    CHECK(marked[13] == 1 && !clipped, "an event without reminders has one hollow mark; lunar words fit (%d clipped)", clipped);
    expect("11月14日 周六"); expect("十月初六"); expect("有事的日子");
    shot("month");
    ink_probe_mark("c3.select.before"); press_text("20"); ink_probe_mark("c3.select.after");
    expect("11月20日"); expect("出差");
    ok("calendar.goto", "{\"date\":\"2026-08-15\"}");
    expect("2026年8月"); on_ui(grid); CHECK(cells == 42 && grey_cells >= 7, "August 2026 is six weeks with greyed neighbours (%d)", grey_cells);
    shot("month-2026-08");
    ink_probe_mark("c3.month.before"); press("calendar.month_next", 0); ink_probe_mark("c3.month.after");
    expect("2026年9月");
    press_text("31"); /* the 31st of August in September's first row: a neighbour's day turns the month */
    expect("2026年8月"); expect("8月31日");
    ok("calendar.goto", "{\"date\":\"2097-08-07\"}");
    ink_lunar_t l; ink_lunar_from_solar(2097, 8, 7, &l);
    char name[32]; ink_lunar_name(&l, name, sizeof name); expect(name); /* lunar.c's date, not Mogo's (LUNAR-DIFF.md) */
    ok("calendar.today", NULL); expect("2026年11月"); expect("11月14日");
}

static void agenda(void) {
    ink_probe_mark("c3.view.before"); press("calendar.view", 0); ink_probe_mark("c3.view.after");
    page("Agenda"); expect("11月16日 周一"); expect("周会"); expect("每周"); expect("提前 10 分钟");
    shot("agenda");
    ink_probe_mark("c3.page.before"); ok("calendar.next", NULL); ink_probe_mark("c3.page.after");
    expect("已改这一次"); shot("agenda-2");
    ink_probe_mark("c3.search.before"); ok("calendar.search", "{\"query\":\"周\"}"); ink_probe_mark("c3.search.after");
    expect("周会"); absent("出差"); shot("search");
    ok("calendar.search", "{\"query\":\"没有的事\"}"); expect("没有找到“没有的事”"); shot("search-empty");
    ok("calendar.search_clear", NULL); expect("周会");
    ok("calendar.view", "{\"view\":\"month\"}"); page("Calendar");
}

static void day_pages(void) {
    ok("calendar.day", "{\"date\":\"2026-02-17\"}"); page("Day");
    CHECK(AI("time.almanac", "{\"date\":\"2026-02-17\"}") && g_rc.status == INK_RC_DONE, "time.almanac answers");
    static char almanac[4096]; snprintf(almanac, sizeof almanac, "%s", g_result);
    ink_almanac_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    CHECK(a && ink_almanac_date(2026, 2, 17, a), "the almanac has 2026-02-17");
    if (a) {
        char yi[256]; snprintf(yi, sizeof yi, "宜 %s", a->yi); expect(yi);
        char ji[160]; snprintf(ji, sizeof ji, "忌 %s", a->ji); expect(ji);
        CHECK(strstr(almanac, a->yi) && strstr(almanac, a->ji), "the page and time.almanac say the same");
        shot("day");
        ok("calendar.almanac", NULL); page("Almanac");
        char line[160]; snprintf(line, sizeof line, "冲%s · 煞%s", a->clash, a->sha); expect(line);
        snprintf(line, sizeof line, "财神 %s · 喜神 %s · 福神 %s", a->wealth, a->joy, a->fortune); expect(line);
        snprintf(line, sizeof line, "值神 %s（%s）", a->guardian, a->guardian_type); expect(line);
        snprintf(line, sizeof line, "建除 %s日", a->officer); expect(line);
        expect("这些是民俗说法"); absent(a->lunar_text);
        shot("almanac");
        press("shell.back", 0);
    }
    osal_free(a);
    ok("calendar.day", "{\"date\":\"1999-12-31\"}"); expect("老黄历只有 2000–2099 年"); expect("农历冬月廿四"); shot("day-1999");
    fail_almanac = true; ok("calendar.day_next", NULL); expect("这天的老黄历查不到"); expect("农历"); fail_almanac = false;
    ok("calendar.day", "{\"date\":\"2026-11-20\"}"); expect("出差"); shot("day-trip");
    press_text("20:00  出差"); page("EventInfo"); expect("出差"); expect("提前 1 天、提前 1 小时"); expect("到 11月22日 18:00");
    shot("event-info");
    press("shell.back", 0); press("shell.back", 0); page("Calendar");
}

static void editor(void) {
    char args[160]; snprintf(args, sizeof args, "{\"id\":%u,\"occurrence\":\"2026-11-30 10:00\"}", (unsigned)id_of("周会"));
    ok("calendar.event", args); page("EventInfo"); expect("每周的周一，到 12月31日为止"); shot("event-info-weekly");
    press("reminder.open", 0); page("ReminderEdit"); expect("2026-11-30 10:00"); shot("edit");
    press("calendar.repeat", 0); page("RepeatRule"); shot("repeat");
    ok("reminder.draft", "{\"repeat\":2}"); CHECK(g_rc.status == INK_RC_DONE, "draft %s %s", ink_rc_name(g_rc.status), g_rc.detail); page("RepeatDetail"); shot("repeat-detail");
    ink_probe_mark("c3.step.before"); press("reminder.draft", 1); ink_probe_mark("c3.step.after"); expect("每 2 周");
    press("reminder.draft", 0); expect("每 1 周");
    press("shell.back", 0); page("ReminderEdit");
    press("calendar.reminders", 0); page("Reminders"); shot("reminders");
    press("calendar.reminder_add", 0); page("ReminderAdd"); shot("reminder-add");
    press_text("提前 1 天"); page("Reminders"); expect("提前 1 天");
    press("shell.back", 0);
    press("reminder.time", 0); page("ReminderTime"); shot("picker");
    ink_probe_mark("c3.picker.before"); press("reminder.draft", 7); ink_probe_mark("c3.picker.after"); expect("时 11");
    press("shell.back", 0); page("ReminderEdit");
    press("reminder.save", 0); page("Scope"); expect("只改这一次"); expect("这次和以后"); shot("scope");
    press("reminder.save", 0); /* 只改这一次 */
    page("Calendar");
    ink_event_t e; int c = 0; bool moved = false;
    while (ink_event_next(&c, &e) == 1) moved |= e.parent && e.hour == 11 && e.nleads == 2;
    CHECK(moved, "touch saved this one apart, at 11:00 with two reminders");
    ok("reminder.new", "{\"date\":\"2026-11-25\"}"); page("ReminderEdit");
    press("reminder.save", 0); expect("标题不能空"); shot("edit-error");
    press("calendar.edit_back", 0); page("Calendar");
    ok("reminder.new", "{\"date\":\"2026-11-25\"}"); press("reminder.draft", 0); /* all day: a change */
    press("calendar.edit_back", 0); page("Leave"); shot("leave");
    press("calendar.edit_discard", 0); page("Calendar");
}

static void delete_undo(void) {
    char args[160]; snprintf(args, sizeof args, "{\"id\":%u,\"occurrence\":\"2026-12-07 10:00\"}", (unsigned)id_of("周会"));
    ok("calendar.select", "{\"day\":7,\"date\":\"2026-12-07\"}");
    ok("calendar.event", args);
    ink_probe_mark("c3.delete.before"); press("calendar.delete", 0); page("Scope"); expect("只删这一次"); shot("scope-delete");
    press("calendar.delete", 0); ink_probe_mark("c3.delete.after");
    page("Calendar"); expect("撤销");
    CHECK(AI("calendar.list", "{\"from\":\"2026-12-07\",\"until\":\"2026-12-08\"}") && !strstr(g_result, "周会"), "12-07 is gone");
    shot("undo");
    ink_probe_mark("c3.undo.before"); press("calendar.undo", 0); ink_probe_mark("c3.undo.after");
    CHECK(AI("calendar.list", "{\"from\":\"2026-12-07\",\"until\":\"2026-12-08\"}") && strstr(g_result, "周会"), "undo brings 12-07 back");
    absent("撤销");
}

static void settings_files(void) {
    ok("calendar.settings", NULL); page("CalendarSettings"); expect("周一（点一下换成周日）"); expect("提前 10 分钟"); shot("settings");
    ok("calendar.export_now", NULL); delay_ms(3000); quiet(); expect("已导出");
    ink_probe_count("test.exported");
    char path[96] = ""; on_ui(read_snapshot); const char *p = strstr(snapshot, "/me/");
    if (p) { snprintf(path, sizeof path, "%s", p); char *q = strpbrk(path, "\" "); if (q) *q = 0; }
    char args[160]; snprintf(args, sizeof args, "{\"path\":\"%s\"}", path);
    ok("calendar.import_run", args); delay_ms(3000); quiet();
    page("ImportResult"); expect("已存在"); shot("import-result");
    press("shell.back", 0); press("calendar.settings", 0); expect("周日（点一下换成周一）");
    press("shell.back", 0); page("Calendar"); on_ui(grid); CHECK(cells == 42, "a Sunday-first month is six weeks too");
    ok("settings.set", "{\"id\":\"calendar.week_start\",\"value\":0}");
}

static void minute(void) {
    ok("calendar.open", NULL);
    ink_time_set(at(2026, 11, 14, 8, 59) + 58, false); quiet(); delay_ms(500);
    ink_probe_mark("c3.minute.before"); delay_ms(4000); quiet(); ink_probe_mark("c3.minute.after");
    ink_probe_mark("c3.passive.before"); ai("reminder.add", "{\"at\":\"2026-11-27 09:00\",\"title\":\"体检\",\"reminders\":[]}"); quiet(); ink_probe_mark("c3.passive.after");
    on_ui(grid); CHECK(marked[27] == 1, "a new event marks its cell");
}

static void host_scene(void *arg) { (void)arg; const char *s = getenv("INK_CALENDAR_PHASE"); snprintf(scene, sizeof scene, "%s", s ? s : "default"); }
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home(); quiet();
    ink_time_set(at(2026, 11, 14, 8, 0), false);
    snprintf(owner, sizeof owner, "%s", ink_member_current()->id);
    if (!strcmp(scene, "child")) { CHECK(ink_member_switch("kid") == 0, "the child"); quiet(); }
    else ok("settings.set", !strcmp(scene, "big") ? "{\"id\":\"display.font_size\",\"value\":3}" : "{\"id\":\"display.font_size\",\"value\":1}");
    fixtures(); month(); agenda(); day_pages(); editor(); delete_undo();
    if (!strcmp(scene, "default")) {
        settings_files(); minute();
        on_ui(font_cover);
        CHECK(missing_other == 0, "every almanac character is in the UI font but the known one (%d others missing)", missing_other);
        CHECK(missing_chars > 0, "the known exception 磑 is still missing: remove this exception once CMakeLists.txt:372 has it (%d characters checked, 磑 on %d days)", checked, seen);
        terms();
        char note[128]; snprintf(note, sizeof note, "solar terms lunar.c vs Mogo 2000-2099: %d days differ, first %s", term_diff, term_first);
        ink_log(INK_LOG_WARN, "calendar-3", "%s", note);
        ink_probe_value("c3.term_diff", term_diff);
    } else settings_files();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_scene, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "calendar-3-pages", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
