/* lock-styles-1: Mogo's lock styles on the real lock screen, with the real settings, reader, almanac and members. */
#include "ink/ui_shell.h"
#include "ink/sys/time.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

static char control[16];
static void host_control(void *arg) {
    (void)arg; const char *p = getenv("INK_LOCK_STYLES_CASE"); snprintf(control, sizeof control, "%.15s", p ? p : "");
}
#include "../system/ui/lock_mark.c"

static char snapshot[8192], frame_name[64];
static int snapshot_len;
static bool captured;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 20000), "lock display settles"); }
static void inspect(void) { snapshot_len = ink_ui_snapshot(snapshot, sizeof snapshot, NULL); }
static bool has(const char *text) {
    on_ui(inspect);
    return snapshot_len > 0 && strstr(snapshot, text) != NULL;
}
static void capture(void *arg) { sim_display_snapshot(arg); captured = true; }
static void frame(const char *name) {
    quiet(); snprintf(frame_name, sizeof frame_name, "%s", name); captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), capture, frame_name);
    CHECK(WAIT_FOR(captured, 1000), "capture %s", name);
}
static void setting(const char *id, int value) {
    ink_receipt_t rc;
    CHECK(ink_setting_set(id, value, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "setting %s=%d", id, value); quiet();
}
static void date(int year, int month, int day, int hour, int minute) {
    ink_tm_t t = {.year = year, .month = month, .day = day, .hour = hour, .min = minute};
    ink_time_set(ink_time_from_local(&t), false); quiet();
}
static double updates(void) {
    return sim_metric_value("display.updates.GC16") + sim_metric_value("display.updates.GL16") +
           sim_metric_value("display.updates.DU") + sim_metric_value("display.updates.A2");
}
static void lock(void) { CHECK(BY_HAND("shell.lock", NULL), "lock"); quiet(); }
static void wake(void) { key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_power_state() != INK_PWR_LOCKED, 5000), "wake"); quiet(); }
static bool drawn(void) { return has("画面上写着"); }
static void choose(int style, ink_source_t src) {
    char args[96]; snprintf(args, sizeof args, "{\"id\":\"display.lock_style\",\"value\":%d}", style);
    CHECK(call_as("settings.set", args, src) && g_rc.status == INK_RC_DONE, "select style %d (%s)", style, src == INK_SRC_AI ? "AI" : "touch");
    quiet();
}
static void show_style(int style) { choose(style, INK_SRC_AI); lock(); CHECK(WAIT_FOR(drawn(), 30000), "style %d is drawn: %.60s", style, snapshot); quiet(); }
static void read_book(const char *path) {
    char args[128]; snprintf(args, sizeof args, "{\"title\":\"\",\"path\":\"%s\"}", path);
    CHECK(AI("reader.open_book", args) && (g_rc.status == INK_RC_DONE || g_rc.status == INK_RC_QUEUED), "reader opens %s", path);
    CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "book"), 30000), "reader page"); quiet();
    for (int i = 0; i < 3; i++) { CHECK(AI("reader.next_page", NULL), "turn"); quiet(); }
    CHECK(BY_HAND("system.home", NULL), "home"); quiet();
}
static void read_some(void) { read_book("/me/books/星星邮差.epub"); }

/* Seven styles (six new, and the wallpaper) at the default and the largest text size, with something to show. */
static void wallpaper(void) { /* a photo on the card, chosen the way the settings choose it */
    uint32_t cursor = 0, id = 0; ink_fs_entry_t e;
    while (ink_fs_index("/family", INK_FS_IMAGE, &cursor, &e) == 1) if (strstr(e.path, "SYNTH_WALL")) id = e.id;
    CHECK(id != 0, "the wallpaper is in the card index");
    setting("display.wallpaper_file", (int)id);
    CHECK(WAIT_FOR(ink_wallpaper_ready(), 20000), "the wallpaper is decoded");
}
static void styles(void) {
    read_some();
    wallpaper();
    date(2026, 2, 17, 8, 30);
    for (int size = 1; size <= 3; size += 2) {
        setting("display.font_size", size);
        for (int style = INK_LOCK_TICKET; style < INK_LOCK_FACE_COUNT; style++) {
            show_style(style);
            CHECK(has(ink_lock_face_choices[style]), "snapshot names style %d", style);
            if (style == INK_LOCK_TICKET) CHECK(strstr(snapshot, "星星邮差") && strstr(snapshot, "已读"), "ticket names the book and progress");
            if (style == INK_LOCK_DIGITAL) CHECK(strstr(snapshot, "时间：08:") && strstr(snapshot, "2月17日"), "clock says what it drew");
            if (style == INK_LOCK_ALMANAC) CHECK(strstr(snapshot, "宜：") && strstr(snapshot, "正月"), "almanac says Yi and the lunar date");
            if (style == INK_LOCK_LIBRARY || style == INK_LOCK_COLLAGE) CHECK(strstr(snapshot, "星星邮差"), "library names the books");
            char name[64]; snprintf(name, sizeof name, "style-%d-size-%d", style, size); frame(name);
            wake();
        }
        choose(2, INK_SRC_AI); lock(); CHECK(has("壁纸"), "the wallpaper style");
        { char name[64]; snprintf(name, sizeof name, "style-2-size-%d", size); frame(name); } wake();
        setting("display.lock_ticket_bg", 1); show_style(INK_LOCK_TICKET);
        char name[64]; snprintf(name, sizeof name, "ticket-wallpaper-size-%d", size); frame(name); wake();
        setting("display.lock_ticket_bg", 0);
    }
    setting("display.font_size", 1);
}

/* The almanac on the lock and time.almanac say the same, word for word. */
static char json[4096];
static void same_day(int y, int m, int d) {
    date(y, m, d, 9, 0);
    char args[48]; snprintf(args, sizeof args, "{\"date\":\"%04d-%02d-%02d\"}", y, m, d);
    CHECK(AI("time.almanac", args) && g_rc.status == INK_RC_DONE, "time.almanac %s", args);
    snprintf(json, sizeof json, "%s", g_result);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    CHECK(j && !ink_json_parse(j, json, strlen(json)), "almanac JSON");
    show_style(INK_LOCK_ALMANAC);
    static const char *const keys[] = {"yi", "ji", "clash", "sha"}, *const labels[] = {"宜：", "忌：", "冲：", "煞："};
    for (int i = 0; j && i < 4; i++) {
        char value[512], want[600];
        CHECK(ink_json_str(j, ink_json_find(j, 0, keys[i]), value, sizeof value) >= 0, "field %s", keys[i]);
        snprintf(want, sizeof want, "%s%s\\n", labels[i], value[0] ? value : "无");
        CHECK(strstr(snapshot, want) != NULL, "lock and time.almanac agree on %s for %s: wanted %.80s in %.60s", keys[i], args, want, snapshot);
    }
    osal_free(j);
    char name[48]; snprintf(name, sizeof name, "almanac-%04d-%02d-%02d", y, m, d); frame(name);
    wake();
}
static void almanac(void) {
    same_day(2026, 2, 17);   /* spring festival */
    same_day(2023, 3, 22);   /* the first day of a leap second month */
    same_day(2026, 11, 22);  /* a solar term (Xiaoxue) */
}

/* Refreshes: the minute changes only the clock's area; the same minute nothing; midnight and the zone redraw once. */
static double area(const char *edge) { return sim_metric_last(edge); }
static void refresh(void) {
    read_some();
    date(2026, 11, 11, 8, 30);
    show_style(INK_LOCK_DIGITAL); frame("minute-before");
    double before = updates(), changed = sim_metric_value("lock.drawn.changed");
    on_minute(NULL, 0, "time.minute"); quiet();
    CHECK(updates() == before && sim_metric_value("lock.drawn.changed") == changed, "the same minute refreshes nothing");
    ink_time_set(ink_time_now_s() + 60, false); quiet();
    CHECK(WAIT_FOR(sim_metric_value("lock.drawn.changed") > changed, 10000), "a new minute is drawn again"); quiet();
    CHECK(has("08:31"), "snapshot follows the minute");
    double x1 = area("lock.drawn.x1"), y1 = area("lock.drawn.y1"), x2 = area("lock.drawn.x2"), y2 = area("lock.drawn.y2");
    CHECK(x1 >= 160 && y1 >= 288 && x2 <= 543 && y2 <= 959, "the minute's refresh stays in the clock digits (%g,%g)-(%g,%g)", x1, y1, x2, y2);
    CHECK(updates() - before >= 1 && updates() - before <= 2, "one refresh for the minute (%g)", updates() - before);
    frame("minute-after");
    wake();
    for (int style = INK_LOCK_TICKET; style <= INK_LOCK_PERPETUAL; style++) {
        if (style == INK_LOCK_COLLAGE || style == INK_LOCK_DIGITAL) continue;
        date(2026, 11, 11, 23, 59); show_style(style);
        double whole = sim_metric_value("lock.drawn.renders");
        ink_time_set(ink_time_now_s() + 60, false); quiet();
        CHECK(WAIT_FOR(sim_metric_value("lock.drawn.renders") > whole, 15000), "style %d redraws itself at midnight", style); quiet();
        CHECK(has("11月12日") || has("2026-11-12"), "style %d shows the new day", style);
        wake();
    }
    date(2026, 11, 11, 10, 0); show_style(INK_LOCK_ALMANAC);
    double zone = sim_metric_value("lock.drawn.renders");
    ink_receipt_t rc;
    CHECK(ink_setting_set("time.zone", 1, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "change the zone"); quiet();
    CHECK(WAIT_FOR(sim_metric_value("lock.drawn.renders") > zone, 15000), "a zone change draws the style again");
    CHECK(ink_setting_set("time.zone", 0, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "zone back"); quiet();
    wake();
    /* The mini library's minute draws from the books it holds: no scan, and only the time reaches the panel. */
    date(2026, 11, 11, 9, 0); show_style(INK_LOCK_LIBRARY);
    double loads = sim_metric_value("mogo_lock.library_loads"), renders = sim_metric_value("lock.drawn.renders"), shown = updates();
    CHECK(loads >= 1, "the library was loaded for its first draw");
    ink_time_set(ink_time_now_s() + 60, false); quiet();
    CHECK(WAIT_FOR(sim_metric_value("lock.drawn.renders") > renders, 15000), "the library's minute is drawn"); quiet();
    CHECK(has("09:01"), "the library's time follows the minute");
    CHECK(sim_metric_value("mogo_lock.library_loads") == loads, "a minute scans nothing (%g loads)", sim_metric_value("mogo_lock.library_loads") - loads);
    CHECK(area("lock.drawn.y1") >= 150 && area("lock.drawn.y2") <= 300, "the library's minute refresh stays in its time (%g..%g)", area("lock.drawn.y1"), area("lock.drawn.y2"));
    CHECK(updates() - shown >= 1 && updates() - shown <= 2, "one refresh for the library's minute (%g)", updates() - shown);
    wake();
    read_book("/me/books/星星邮差GBK.txt");
    show_style(INK_LOCK_LIBRARY);
    CHECK(WAIT_FOR(sim_metric_value("mogo_lock.library_loads") > loads, 15000), "a book read since loads the library again, without waiting for the minute");
    CHECK(WAIT_FOR(has("星星邮差GBK"), 15000), "the library shows the book read since");
    wake();
}

/* Members: a closed space shows no book, someone else's books never show, a child sees only their own. */
static void privacy(void) {
    read_some();
    for (int style = INK_LOCK_TICKET; style <= INK_LOCK_LIBRARY; style++) {
        if (style != INK_LOCK_TICKET && style != INK_LOCK_COLLAGE && style != INK_LOCK_LIBRARY) continue;
        show_style(style);
        CHECK(strstr(snapshot, "星星邮差"), "owner sees their own book on style %d", style);
        CHECK(ink_member_switch("kid") == 0, "switch to the child while locked"); quiet();
        CHECK(WAIT_FOR(drawn() || has("还没有读过的书"), 30000), "child's lock drawn or says why: %.60s", snapshot);
        CHECK(!strstr(snapshot, "星星邮差"), "the child never sees the adult's book on style %d: %.60s", style, snapshot);
        char name[48]; snprintf(name, sizeof name, "child-style-%d", style); frame(name);
        CHECK(ink_member_switch("owner") == 0, "back to the adult"); quiet();
        wake();
    }
    CHECK(BY_HAND("member.set_pin", "{\"id\":\"owner\",\"new_pin\":\"2468\"}") && g_rc.status == INK_RC_DONE, "person sets a PIN");
    for (int style = INK_LOCK_TICKET; style <= INK_LOCK_LIBRARY; style++) {
        if (style != INK_LOCK_TICKET && style != INK_LOCK_COLLAGE && style != INK_LOCK_LIBRARY) continue;
        show_style(style);
        CHECK(WAIT_FOR(!ink_member_space_open(), 200000), "the grace runs out"); quiet();
        CHECK(WAIT_FOR(has("解锁后显示"), 30000), "closed space says the books come after the unlock on style %d", style);
        CHECK(!strstr(snapshot, "星星邮差") && !strstr(snapshot, "书名"), "no title in the closed snapshot on style %d", style);
        char name[48]; snprintf(name, sizeof name, "closed-style-%d", style); frame(name);
        key(HAL_KEY_POWER); quiet();
        CHECK(BY_HAND("member.unlock", "{\"id\":\"owner\",\"pin\":\"2468\"}") && g_rc.status == INK_RC_DONE, "person unlocks"); quiet();
    }
}

/* Touch and AI choose the same; a tapped book opens after the unlock, as the AI would open it. */
static int tx, ty;
static void says_of(const char *snap, char *out, size_t cap) { /* the value of the "画面上写着" field */
    const char *p = strstr(snap, "\"label\":\"画面上写着\",\"value\":\"");
    out[0] = 0;
    if (!p) return;
    p += strlen("\"label\":\"画面上写着\",\"value\":\"");
    const char *e = strstr(p, "\",\"secret\"");
    size_t n = e ? (size_t)(e - p) : 0;
    if (n >= cap) n = cap - 1;
    memcpy(out, p, n); out[n] = 0;
}
static void center_of(void) { tx = 342; ty = 108 + (420 + 184) * 1108 / 1216; } /* the focused cover, on the panel */
static void same_entry(void) {
    read_some();
    for (int style = INK_LOCK_TICKET; style < INK_LOCK_FACE_COUNT; style++) {
        CHECK(BY_HAND("shell.lock_faces", NULL) && g_rc.status == INK_RC_DONE, "open the picker"); quiet();
        char page[32]; snprintf(page, sizeof page, "{\"page\":%d}", 2);
        if (!has(ink_lock_face_choices[style]) || style > INK_LOCK_COLLAGE) { CHECK(BY_HAND("shell.lock_faces", page), "page two"); quiet(); }
        choose(style, INK_SRC_TOUCH);
        CHECK(ink_setting_get("display.lock_style") == style && has("已选择"), "touch selects style %d", style);
        lock(); CHECK(WAIT_FOR(drawn(), 30000), "drawn after touch");
        static char by_touch[2048], by_ai[2048];
        says_of(snapshot, by_touch, sizeof by_touch);
        wake();
        show_style(style);
        says_of(snapshot, by_ai, sizeof by_ai);
        CHECK(by_touch[0] && (!strcmp(by_touch, by_ai) || style == INK_LOCK_DIGITAL || style == INK_LOCK_LIBRARY),
              "AI and touch reach the same lock for style %d: %.60s | %.60s", style, by_touch, by_ai);
        wake();
    }
    for (int size = 1; size <= 3; size += 2) { /* the picker at both sizes, with each style's own row */
        setting("display.font_size", size);
        static const int chosen[] = {0, INK_LOCK_TICKET, INK_LOCK_LIBRARY};
        for (int i = 0; i < 3; i++) {
            setting("display.lock_style", chosen[i]);
            char page[24]; snprintf(page, sizeof page, "{\"page\":%d}", i ? 2 : 1);
            CHECK(BY_HAND("shell.lock_faces", NULL) && BY_HAND("shell.lock_faces", page), "picker page"); quiet();
            if (i) CHECK(has(i == 1 ? "票根背景" : "迷你书库显示"), "the chosen style's own row shows: %.60s", snapshot);
            char name[48]; snprintf(name, sizeof name, "picker-%d-size-%d", chosen[i], size); frame(name);
            CHECK(BY_HAND("system.home", NULL), "home"); quiet();
        }
    }
    setting("display.font_size", 1);
    setting("display.lock_library", 0);
    show_style(INK_LOCK_LIBRARY);
    on_ui(center_of); tap(tx, ty); quiet();
    CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "book"), 30000), "the tapped book opens after the unlock (page %s)", ink_ui_page_name());
    CHECK(BY_HAND("system.home", NULL), "home"); quiet();
    show_style(INK_LOCK_LIBRARY); wake();
    CHECK(AI("reader.open_book", "{\"title\":\"\",\"path\":\"/me/books/星星邮差.epub\"}") && (g_rc.status == INK_RC_DONE || g_rc.status == INK_RC_QUEUED), "AI opens the same book");
    CHECK(WAIT_FOR(!strcmp(ink_ui_page_name(), "book"), 30000), "same page for the AI"); quiet();
    CHECK(BY_HAND("system.home", NULL), "home"); quiet();
}

/* Old numbers keep their styles; missing pieces are said, never a blank or the page under the lock. */
static void upgrade(void) {
    CHECK(!strcmp(ink_lock_face_choices[5], "月历") && !strcmp(ink_lock_face_choices[0], "时钟和通知"), "0..5 keep their meaning, 0 renamed");
    setting("display.lock_style", 5); lock(); CHECK(has("月历"), "a saved 5 is still the month calendar"); wake();
    date(2026, 11, 11, 8, 30);
    show_style(INK_LOCK_TICKET); CHECK(strstr(snapshot, "已读") != NULL, "ticket drawn"); wake();
}
static void empty(void) { /* no book on the card, no font for the child: each says why */
    date(2026, 11, 11, 8, 30);
    choose(INK_LOCK_TICKET, INK_SRC_AI); lock();
    CHECK(WAIT_FOR(has("还没有读过的书"), 30000), "no reading yet is said on the lock: %.60s", snapshot); frame("ticket-empty"); wake();
    show_style(INK_LOCK_LIBRARY); CHECK(strstr(snapshot, "还没有阅读记录") || strstr(snapshot, "书匣"), "empty library says so"); frame("library-empty"); wake();
    setting("display.lock_library", 1); show_style(INK_LOCK_LIBRARY); frame("library-random-empty"); wake(); setting("display.lock_library", 0);
}
static void nofont(void) {
    date(2026, 11, 11, 8, 30);
    choose(INK_LOCK_DIGITAL, INK_SRC_AI); lock();
    CHECK(WAIT_FOR(has("字体还没放到卡上"), 30000), "a missing font is said on the default lock: %.60s", snapshot);
    CHECK(g_next != NULL, "the default lock shows, not the page below"); frame("no-font"); wake();
}
/* The ticket follows the book read last, every time, and its first lock costs one render and one whole refresh. */
static void latest(void) {
    date(2026, 11, 11, 8, 30);
    read_book("/me/books/星星邮差.epub");
    choose(INK_LOCK_TICKET, INK_SRC_AI);
    double renders = sim_metric_value("lock.drawn.renders"), whole = sim_metric_value("lock.drawn.whole");
    lock(); CHECK(WAIT_FOR(drawn(), 30000), "ticket drawn"); quiet();
    CHECK(strstr(snapshot, "书名：星星邮差\\n") != NULL, "the ticket shows the epub read first: %.80s", strstr(snapshot, "书名") ? strstr(snapshot, "书名") : snapshot);
    CHECK(sim_metric_value("lock.drawn.renders") - renders == 1 && sim_metric_value("lock.drawn.whole") - whole == 1,
          "the first ticket lock is drawn once and replaces the default lock once (%g renders, %g whole)",
          sim_metric_value("lock.drawn.renders") - renders, sim_metric_value("lock.drawn.whole") - whole);
    wake();
    read_book("/me/books/星星邮差GBK.txt");
    lock(); CHECK(WAIT_FOR(has("书名：星星邮差GBK"), 30000), "the ticket follows the book read next: %.80s", strstr(snapshot, "书名") ? strstr(snapshot, "书名") : snapshot);
    wake();
    read_book("/me/books/星星邮差.epub");
    lock(); CHECK(WAIT_FOR(has("书名：星星邮差\\n"), 30000), "and back to the epub when it is read again"); wake();
}

/* The chip does not sleep under a style still being drawn. The background executor is held up 4 s, the way a slow
 * cover decode or library scan holds it: the first sleep after the lock comes only once the style is on the panel,
 * not after the default lock with the render frozen for a whole slice. */
static ink_exec_job_t slow;
static void slow_job(void *arg) { (void)arg; osal_task_delay_us(4000000); }
static void sleep_case(void) {
    date(2026, 11, 11, 8, 30);
    choose(INK_LOCK_ALMANAC, INK_SRC_AI);
    double whole = sim_metric_value("lock.drawn.whole"), slept = sim_metric_value("power.wake.timer");
    ink_power_release(INK_NEED_AWAKE);
    CHECK(ink_exec_post(INK_EXEC_BG, &slow, slow_job, NULL) == 0, "hold the background executor up");
    lock();
    delay_ms(70000); /* no UI calls meanwhile: they would keep the chip awake on their own */
    CHECK(sim_metric_value("power.wake.timer") > slept, "the chip slept while locked");
    double first = sim_metric_last("power.lock_to_sleep_ms");
    CHECK(first >= 4000, "the first sleep waits for the style being drawn (%.0f ms after the lock)", first);
    CHECK(sim_metric_value("lock.drawn.whole") > whole && drawn(), "the style is on the lock: %.60s", snapshot);
    wake();
    ink_power_hold(INK_NEED_AWAKE);
}
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "space opens"); test_start_home();
    ink_power_hold(INK_NEED_AWAKE);
    if (!strcmp(control, "styles")) styles();
    else if (!strcmp(control, "almanac")) almanac();
    else if (!strcmp(control, "refresh")) refresh();
    else if (!strcmp(control, "privacy")) privacy();
    else if (!strcmp(control, "entry")) same_entry();
    else if (!strcmp(control, "upgrade")) { read_some(); upgrade(); }
    else if (!strcmp(control, "empty")) empty();
    else if (!strcmp(control, "nofont")) nofont();
    else if (!strcmp(control, "sleep")) sleep_case();
    else if (!strcmp(control, "latest")) latest();
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_control, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "lock-styles-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start lock styles fixture");
}
