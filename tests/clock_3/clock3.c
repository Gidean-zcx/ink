/* clock-3 on the whole product: the reference paths of APPS.md A04 by touch and again by the AI, with injected time,
 * a picture at each step, the panel updates counted (only the digits while seconds run, one refresh per action),
 * members kept apart, and what survives a restart. INK_CLOCK3_PHASE: main, shots (pictures only) or restart. */
#include <stdlib.h>
#include "ink/hal_display.h"
#include "ink/rotation.h"
#include "ink/ui_shell.h"
#include "ink/sys/interrupt.h"
#include "src/core/lv_obj_draw_private.h"
static bool tracking;
static hal_rect_t allowed[8], bad;
static unsigned nallowed, updates, outside;
static uint32_t tracked(const hal_rect_t *r, const uint8_t *px, int stride, hal_waveform_t wf, uint32_t flags, hal_display_done_fn done, void *ctx) {
    if (tracking) {
        bool in = false; updates++;
        for (unsigned i = 0; i < nallowed; i++)
            in |= r->x >= allowed[i].x && r->y >= allowed[i].y && r->x + r->w <= allowed[i].x + allowed[i].w && r->y + r->h <= allowed[i].y + allowed[i].h;
        if (!in) { outside++; bad = *r; }
    }
    return hal_display_update(r, px, stride, wf, flags, done, ctx);
}
#define hal_display_update tracked
#include "../../system/services/display/display_service.c"
#undef hal_display_update
#define ink_main product_main
#include "../configured_boot.c"
#undef ink_main
#include "../integration/whole_product.h"

static char phase[16], suffix[16], snap[12288], text[160];
static const char *wanted;
static int nth, seen, px, py;
static lv_obj_t *hit;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the page settles"); }
static void find(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), wanted) && seen++ == nth) {
        lv_area_t a; lv_obj_get_coords(o, &a); px = (a.x1 + a.x2) / 2; py = (a.y1 + a.y2) / 2; hit = o;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) { hit = NULL; seen = 0; lv_obj_update_layout(lv_screen_active()); find(lv_screen_active()); find(lv_layer_sys()); }
static bool visible(const char *label) { wanted = label; nth = 0; on_ui(locate); return hit != NULL; }
static void press_nth(const char *label, int index) {
    quiet(); wanted = label; nth = index; on_ui(locate);
    CHECK(hit, "a visible button %s #%d on %s", label, index, ink_ui_page_name());
    if (hit) tap(px, py);
    quiet();
}
static void press(const char *label) { press_nth(label, 0); }
static void read_snap(void) { int n = ink_ui_snapshot(snap, sizeof snap, NULL); if (n <= 0) { snap[0] = 0; fprintf(stderr, "clock3 no snapshot (%d) on %s\n", n, ink_ui_page_name()); } }
static bool has(const char *s) { on_ui(read_snap); return strstr(snap, s) != NULL; }
static int64_t number(const char *key) {
    char k[64]; long long v = -1; snprintf(k, sizeof k, "\"%s\":", key); on_ui(read_snap);
    const char *p = strstr(snap, k);
    CHECK(p && sscanf(p + strlen(k), "%lld", &v) == 1, "the snapshot has %s", key); return v;
}
static const char *field(const char *id) { /* a field's value, or NULL */
    char k[96]; snprintf(k, sizeof k, "{\"id\":\"%s\"", id); on_ui(read_snap);
    const char *p = strstr(snap, k), *v = p ? strstr(p, "\"value\":\"") : NULL;
    if (!v || sscanf(v, "\"value\":\"%159[^\"]", text) != 1) return NULL;
    return text;
}
static bool field_has(const char *id, const char *s) { const char *v = field(id); return v && strstr(v, s); }
static void ok(const char *cap, const char *args) { CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s %s done (%d %s)", cap, args ? args : "", g_rc.status, g_rc.detail); quiet(); }
static void frame(void *name) { sim_display_snapshot(name); }
static void shot(const char *name) {
    static char names[64][40]; static int n;
    quiet(); snprintf(names[n % 64], sizeof names[0], "%s%s", name, suffix);
    vtk_event_at(vtk_now() + SIM_MS(1), frame, names[n++ % 64]); delay_ms(5);
}
static int64_t utc(int y, int m, int d, int h, int mi) { ink_tm_t t = {.year = y, .month = m, .day = d, .hour = h, .min = mi}; return ink_time_from_local(&t); }
static void set_time(int64_t s) { char a[48]; snprintf(a, sizeof a, "{\"utc_s\":%lld}", (long long)s); ok("time.set", a); }
static void setting(const char *id, int v) { char a[96]; snprintf(a, sizeof a, "{\"id\":\"%s\",\"value\":%d}", id, v); ok("settings.set", a); }
static void wait_us(uint64_t us) { uint64_t until = osal_now_us() + us; CHECK(WAIT_FOR(osal_now_us() >= until, (int)(us / 1000) + 200), "time passes"); }
static void clear_of_minute(void) { while (osal_wall_us() / 1000000 % 60 >= 50) delay_ms(250); quiet(); }
static void type_name(const char *focus, const char *name) {
    char a[96]; ok(focus, NULL); ok("text.select", "{\"start\":0,\"end\":1000}");
    snprintf(a, sizeof a, "{\"text\":\"%s\"}", name); ok("text.insert", a); ok("text.submit", NULL);
}
static void open_clock(void) { ok("system.home", NULL); ok("app.open", "{\"id\":\"clock\"}"); }
/* the panel updates while `ms` pass may only touch the given labels */
static hal_rect_t area_of(lv_obj_t *o) {
    lv_area_t a; lv_obj_get_coords(o, &a); int32_t e = lv_obj_get_ext_draw_size(o); lv_area_increase(&a, e, e);
    hal_panel_info_t p; hal_display_get_info(&p); int x0, y0, x1, y1;
    ink_logical_to_native(p.width, p.height, p.rotation_deg, a.x1, a.y1, &x0, &y0);
    ink_logical_to_native(p.width, p.height, p.rotation_deg, a.x2, a.y2, &x1, &y1);
    return (hal_rect_t){x0 < x1 ? x0 : x1, y0 < y1 ? y0 : y1, abs(x1 - x0) + 1, abs(y1 - y0) + 1};
}
static void allow(void) { if (hit && nallowed < 8) allowed[nallowed++] = area_of(hit); }
static void allow_label(const char *label, int index) { wanted = label ? label : ""; nth = index; on_ui(locate); CHECK(hit, "label %s", wanted); on_ui(allow); }
static void watch_updates(int ms) { updates = outside = 0; tracking = true; delay_ms(ms); tracking = false; }
static unsigned refreshes(const char *what, void (*act)(void)) { /* panel refreshes for one action, the minute clear of it */
    clear_of_minute(); uint32_t before = ink_display_commits(); act(); quiet();
    unsigned n = ink_display_commits() - before;
    fprintf(stderr, "clock3 refreshes %s%s: %u\n", what, suffix, n);
    CHECK(n == 1, "%s costs one refresh: %u", what, n); return n;
}
static void tab_world(void) { press_nth("时钟", 1); } /* the first is the title */
static void tab_alarm(void) { press("闹钟"); }
static void tab_watch(void) { press("秒表"); }
static void tab_timer(void) { press("计时"); }
static int timer_id(const char *title) { /* from the timers in the snapshot */
    char k[64]; snprintf(k, sizeof k, "\"title\":\"%s\"", title); on_ui(read_snap);
    const char *p = strstr(snap, k);
    while (p && p > snap && strncmp(p, "{\"id\":", 6)) p--;
    int id = -1; CHECK(p && sscanf(p, "{\"id\":%d", &id) == 1, "timer %s listed", title); return id;
}
static void ints(const char *key, int *out, int max, int *n) { /* every "key":N in the snapshot, in order */
    char k[32]; snprintf(k, sizeof k, "\"%s\":", key); on_ui(read_snap); *n = 0;
    for (char *p = strstr(snap, k); p && *n < max; p = strstr(p + 1, k)) out[(*n)++] = atoi(p + strlen(k));
}
static void world(bool ai) { /* APPS.md A04: a city in another day */
    set_time(utc(2026, 11, 20, 8, 0)); open_clock();
    if (ai) ok("clock.world", NULL); else { tab_world(); shot("Clock-world-empty"); }
    CHECK(has("\"tab\":\"world\"") && field_has("local_time", "08:00") && field_has("date", "11月20日 周五"), "local time and date");
    if (ai) { ok("clock.world_add", "{\"zone\":7}"); ok("clock.world_add", "{\"zone\":2}"); ok("clock.world_add", "{\"zone\":0}"); }
    else {
        press("加城市"); shot("ClockWorldPick");
        press("日本 · 东京"); press("中国 · 上海"); key(INK_KEY_NEXT); quiet();
        CHECK(has("\"page_number\":2") && has("\"total\":9"), "nine cities over two pages");
        press("美国 · 纽约"); CHECK(field_has("America/New_York", "已选"), "the row says it is added"); shot("ClockWorldPick-chosen");
        press("完成");
    }
    CHECK(!strcmp(ink_ui_page_name(), "Clock") && field_has("America/New_York", "19:00 · 昨天 · 比这里晚 13 小时"), "New York: yesterday, 19:00, 13 hours behind: %s", text);
    CHECK(field_has("Asia/Tokyo", "09:00 · 比这里早 1 小时") && field_has("Asia/Shanghai", "08:00 · 和这里同一时间"), "ahead and the same time");
    if (!ai) shot("Clock-world");
    set_time(utc(2027, 7, 15, 8, 0));
    CHECK(field_has("America/New_York", "20:00 · 昨天 · 比这里晚 12 小时"), "daylight saving: 12 hours: %s", text);
    if (ai) ok("clock.world_edit", "{\"zone\":7}"); else press("美国 · 纽约");
    CHECK(!strcmp(ink_ui_page_name(), "ClockCity") && !ink_text_focused(), "the city page opens without the keyboard");
    if (!ai) shot("ClockCity");
    if (ai) ok("clock.world_title", "{\"zone\":7,\"title\":\"老妹家\"}"); else { type_name("clock.world_name", "老妹家"); press("保存"); }
    CHECK(!strcmp(ink_ui_page_name(), "Clock") && has("\"label\":\"老妹家\"") && has("\"name\":\"老妹家\""), "the new name on the list");
    if (ai) ok("clock.world_title", "{\"zone\":7,\"title\":\"\"}"); else { press("老妹家"); type_name("clock.world_name", ""); press("保存"); }
    CHECK(!has("老妹家") && has("\"name\":\"美国 · 纽约\""), "an empty name is the table's again");
    if (ai) { ok("clock.world_edit", "{\"zone\":2}"); ok("clock.world_remove", "{\"zone\":2}"); }
    else { press("日本 · 东京"); press("删除这个城市"); }
    CHECK(!strcmp(ink_ui_page_name(), "Clock") && !field("Asia/Tokyo"), "removed from its own page");
    if (!ai) { ok("clock.world_title", "{\"zone\":7,\"title\":\"老妹家\"}"); return; } /* kept for the restart */
    ok("clock.world_add", "{\"zone\":1}"); ok("clock.world_add", "{\"zone\":3}"); ok("clock.world_add", "{\"zone\":4}"); ok("clock.world_add", "{\"zone\":5}");
    CHECK(AI("clock.world_add", "{\"zone\":6}") && g_rc.status == INK_RC_FAILED && strstr(g_rc.detail, "full"), "a seventh city is refused");
    for (int z = 1; z <= 5; z++) { char a[16]; snprintf(a, sizeof a, "{\"zone\":%d}", z); if (z != 2) ok("clock.world_remove", a); }
    ok("clock.world_title", "{\"zone\":7,\"title\":\"老妹家\"}"); /* kept for the restart */
}
static void alarm(bool ai) { /* a weekday alarm seen on Friday evening, ringing on Monday, snoozed */
    set_time(utc(2026, 11, 20, 21, 0)); open_clock();
    if (ai) { ok("clock.tab", "{\"tab\":\"alarm\"}"); ok("clock.new", NULL); ok("clock.draft", "{\"hour\":7,\"minute\":0,\"weekdays\":62}"); }
    else {
        tab_alarm(); press("+ 加闹钟");
        CHECK(!strcmp(ink_ui_page_name(), "ClockAlarm") && !ink_text_focused() && field_has("repeat", "只响一次 · 明天"), "the editor: no keyboard, once tomorrow");
        press("时＋"); CHECK(field_has("time", "08:00"), "hour plus"); press("时-");
        press("工作日"); CHECK(field_has("repeat", "工作日"), "weekdays"); shot("ClockAlarm-new");
    }
    ok("clock.save", NULL);
    CHECK(!strcmp(ink_ui_page_name(), "Clock") && has("下一个闹钟：周一 7:00，还有 58 小时") && has("\"next_alarm\":\"周一 7:00，还有 58 小时\""), "the alarm tab says Monday and how long");
    if (!ai) shot("Clock-alarm");
    if (ai) ok("clock.world", NULL); else tab_world();
    CHECK(has("下一个闹钟：周一 7:00") && !has("还有 58"), "the clock tab says Monday 7:00");
    if (ai) return;
    ink_alarm_t a; int cursor = 0; int64_t monday = utc(2026, 11, 23, 7, 0);
    while (ink_alarm_next(&cursor, INK_ALARM, &a) == 1) CHECK(a.next_s == monday, "scheduled Monday 7:00");
    set_time(monday); CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 5000), "Monday 7:00 rings"); quiet();
    press("再睡 10 分钟"); CHECK(WAIT_FOR(strcmp(active_state(), "alarm"), 5000), "snoozed");
    set_time(monday + 600); CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 5000), "7:10 rings again");
    ok("alarm.dismiss", NULL); CHECK(WAIT_FOR(strcmp(active_state(), "alarm"), 5000), "dismissed");
}
static void lap_check(uint64_t begin_lo, uint64_t begin_hi, const uint64_t *lo, const uint64_t *hi) {
    int lap[8], lap_ms[8], total[8], n, m, k;
    ints("lap", lap, 8, &n); ints("lap_ms", lap_ms, 8, &m); ints("total_ms", total, 8, &k);
    CHECK(n == 3 && m == 3 && k == 3, "three laps listed");
    for (int i = 0; i < n && i < m && i < k; i++) {
        int at = lap[i] - 1; int64_t low = (int64_t)(lo[at] - begin_hi) / 1000, high = (int64_t)(hi[at] - begin_lo) / 1000;
        CHECK(total[i] + 10 >= low && total[i] <= high + 10, "lap %d total %d ms in [%lld, %lld]", lap[i], total[i], (long long)low, (long long)high);
        int64_t plo = at ? (int64_t)(lo[at - 1] - begin_hi) / 1000 : 0, phi = at ? (int64_t)(hi[at - 1] - begin_lo) / 1000 : 0;
        CHECK(lap_ms[i] + 10 >= low - phi && lap_ms[i] <= high - plo + 10, "lap %d time %d ms", lap[i], lap_ms[i]);
    }
}
static void stopwatch(bool ai) { /* start, three laps of different length, pause: each lap and total to 0.01 s, fastest first */
    open_clock(); if (ai) ok("clock.stopwatch", NULL); else tab_watch();
    if (ai) ok("clock.stopwatch_reset", NULL); else shot("Clock-stopwatch-empty");
    CHECK(number("elapsed_ms") == 0 && !has("\"running\":true"), "cleared");
    uint64_t lo[3], hi[3], begin_lo = osal_now_us(); if (ai) ok("clock.stopwatch_start", NULL); else press("开始");
    uint64_t begin_hi = osal_now_us(); static const int GAP[] = {1230, 2470, 710};
    for (int i = 0; i < 3; i++) { wait_us((uint64_t)GAP[i] * 1000); lo[i] = osal_now_us(); if (ai) ok("clock.stopwatch_lap", NULL); else press("计次"); hi[i] = osal_now_us(); }
    if (ai) ok("clock.stopwatch_pause", NULL); else press("暂停");
    CHECK(has("\"running\":false") && field("elapsed") && strchr(text, '.'), "paused to hundredths: %s", text);
    lap_check(begin_lo, begin_hi, lo, hi);
    CHECK(field_has("第 3 圈", "单圈") && field_has("第 3 圈", "累计"), "a lap row has its own time and the total");
    if (ai) { ok("clock.lap_sort", "{\"by\":\"lap_time\"}"); ok("clock.lap_sort", "{\"by\":\"lap_time\"}"); } else { press("单圈"); press("单圈 ↓"); }
    int ms[8], n; ints("lap_ms", ms, 8, &n);
    CHECK(has("\"sort\":\"lap_time_asc\"") && n == 3 && ms[0] <= ms[1] && ms[1] <= ms[2], "lap time ascending: the fastest first (%d %d %d)", ms[0], ms[1], ms[2]);
    if (!ai) { shot("Clock-stopwatch"); press("单圈 ↑"); press("圈次"); }
    else { ok("clock.lap_sort", "{\"by\":\"lap\"}"); CHECK(has("\"sort\":\"lap_desc\""), "a new column starts descending"); }
}
static bool visible_time(int seconds) { /* m:ss within a second either way */
    for (int d = -1; d <= 1; d++) { char t[8]; snprintf(t, sizeof t, "%d:%02d", (seconds + d) / 60, (seconds + d) % 60); if (visible(t)) return true; }
    return false;
}
static bool timer_is(int t, const char *what) { /* in the timers list of the snapshot */
    char k[32]; snprintf(k, sizeof k, "{\"id\":%d,", t); on_ui(read_snap);
    const char *p = strstr(snap, k), *e = p ? strchr(p, '}') : NULL;
    if (!e) return false;
    snprintf(text, sizeof text, "%.*s", (int)(e - p), p);
    return strstr(text, what) != NULL;
}
static void allow_union(void) { /* one panel rectangle per frame: the two rows' numbers and what lies between them */
    if (nallowed < 2) return;
    hal_rect_t a = allowed[0], b = allowed[1]; int x0 = a.x < b.x ? a.x : b.x, y0 = a.y < b.y ? a.y : b.y;
    int x1 = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w, y1 = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
    allowed[2] = (hal_rect_t){x0, y0, x1 - x0, y1 - y0}; nallowed = 3;
}
static void allow_row(const char *id) { char now[16] = ""; if (field(id)) sscanf(text, "%15s", now); allow_label(now, 0); }
static void counter_plus(void) { press("分＋"); }
static void counter_minus(void) { press("分－"); }
static void timer(bool ai) { /* A04: 1:05, pause at 30 s, ten seconds, resume, rings with its name; the other keeps going; quiet after 60 s off the page */
    open_clock(); if (ai) ok("clock.timer", NULL); else { tab_timer(); shot("Clock-timer"); }
    CHECK(has("\"seconds\":300") && has("\"current_timer\""), "a member's first visit makes one five-minute timer");
    const char *name = ai ? "面条" : "泡面", *other = ai ? "鸡蛋" : "煮蛋";
    if (ai) { ok("clock.timer_new", NULL); ok("clock.timer_set", "{\"seconds\":65}"); }
    else {
        press("+ 加计时器"); CHECK(!strcmp(ink_ui_page_name(), "ClockTimerEdit") && !ink_text_focused() && field_has("duration", "00:05:00"), "a new timer: five minutes, no keyboard");
        for (int i = 0; i < 4; i++) refreshes("counter minus", counter_minus);
        for (int i = 0; i < 5; i++) press("秒＋");
        CHECK(field_has("duration", "00:01:05"), "1:05 set with the counters: %s", text);
    }
    type_name("clock.timer_name", name); if (!ai) shot("ClockTimerEdit-new");
    ok("clock.timer_save", NULL);
    ok("clock.timer_new", NULL); ok("clock.timer_set", "{\"seconds\":180}"); type_name("clock.timer_name", other); ok("clock.timer_save", NULL);
    int t = timer_id(name), o = timer_id(other); char a[32], b[32], id[16];
    snprintf(a, sizeof a, "{\"id\":%d}", t); snprintf(b, sizeof b, "{\"id\":%d}", o); snprintf(id, sizeof id, "timer:%d", t);
    uint64_t start = osal_now_us();
    if (ai) ok("clock.timer_start", a); else press_nth("开始", 1);
    ok("clock.timer_start", b);
    { char want[48]; snprintf(want, sizeof want, "\"remaining_seconds\":%d,\"state\":\"running\"", 65 - (int)((osal_now_us() - start) / 1000000));
      CHECK(timer_is(t, want), "1:05 running: %s", text); }
    wait_us(30000000 - (osal_now_us() - start));
    if (ai) ok("clock.timer_pause", a); else press("暂停");
    int rem = (int)number("remaining_seconds"); /* the first timer listed is the seeded one: read this one's */
    { char k[48]; snprintf(k, sizeof k, "{\"id\":%d,", t); const char *p = strstr(snap, k), *r = p ? strstr(p, "\"remaining_seconds\":") : NULL; rem = r ? atoi(r + 20) : -1; }
    CHECK(timer_is(t, "\"state\":\"paused\"") && (rem == 35 || (!ai && rem == 36)), "paused at 30 s with 35 s left (touch: within its delay): %s", text);
    if (!ai) CHECK(field_has(id, ":3") && field_has(id, " · 已暂停"), "its row says paused: %s", text);
    wait_us(10000000);
    { char want[48]; snprintf(want, sizeof want, "\"remaining_seconds\":%d,\"state\":\"paused\"", rem); CHECK(timer_is(t, want), "a pause holds: %s", text); }
    uint64_t resumed = osal_now_us();
    if (ai) ok("clock.timer_start", a); else press("继续");
    uint64_t resumed_hi = osal_now_us();
    if (!ai) shot("Clock-timer-running");
    CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 40000), "it rings");
    CHECK(osal_now_us() + 100000 >= resumed_hi + (uint64_t)(rem - 1) * 1000000 && osal_now_us() <= resumed + (uint64_t)rem * 1000000 + 300000,
          "rings when 65 s of running are up: %lld ms after resuming, %d s were left", (long long)((osal_now_us() - resumed) / 1000), rem);
    uint64_t rang = osal_now_us(); quiet();
    CHECK(has("计时结束") && has(name), "the card names the timer");
    if (!ai) CHECK(visible("已到点") && visible("0:00"), "its row: finished, 0:00"); /* under the card; the AI's timer is on page 2 */
    int other_left = 180 - (int)((osal_now_us() - start) / 1000000);
    if (!ai) CHECK(visible_time(other_left), "the other keeps going (%d s left)", other_left);
    if (!ai) { /* the finished row no longer redraws every second; only the running one's numbers do */
        shot("ClockTimerRing");
        nallowed = 0; other_left = 180 - (int)((osal_now_us() - start) / 1000000); visible_time(other_left); on_ui(allow); watch_updates(3100);
        CHECK(outside == 0 && nallowed == 1, "60 s of ringing: no second-by-second refresh of the finished row (%u updates, %u outside)", updates, outside);
    }
    ok("system.home", NULL); if (!ai) ink_power_lock();
    wait_us(rang + 60500000 - osal_now_us());
    CHECK(AI("interrupt.list", NULL) && strstr(g_result, name) && strstr(g_result, "\"ringing\":false") && !strstr(g_result, "\"ringing\":true"), "quiet after 60 s off the page%s, the card stays", ai ? "" : " and locked");
    press("再计一次"); CHECK(WAIT_FOR(strcmp(active_state(), "alarm"), 5000), "the card's again ends it");
    if (!ai) { key(HAL_KEY_POWER); CHECK(WAIT_FOR(ink_power_state() != INK_PWR_LOCKED, 3000), "unlocked"); quiet(); }
    open_clock(); if (ai) ok("clock.timer", NULL); else tab_timer();
    { int left = 65 - (int)((osal_now_us() - rang) / 1000000) + 60; char want[48];
      snprintf(want, sizeof want, "\"state\":\"running\""); CHECK(timer_is(t, want) && strstr(text, "\"seconds\":65"), "running again from 1:05: %s", text); (void)left; }
    if (ai) { char k[48]; snprintf(k, sizeof k, "{\"id\":%d,", o); CHECK(strstr(snap, k) && strstr(strstr(snap, k), "\"state\":\"running\""), "the other still running"); }
    if (!ai) { /* two running: only their numbers */
        quiet(); nallowed = 0; allow_row(id); { char oid[16]; snprintf(oid, sizeof oid, "timer:%d", o); allow_row(oid); } allow_union();
        watch_updates(3100);
        CHECK(updates >= 2 && outside == 0, "two running timers: only their digits (%u updates, %u outside, %d,%d %dx%d)", updates, outside, bad.x, bad.y, bad.w, bad.h);
    }
    ok("clock.timer_cancel", a); ok("clock.timer_cancel", b);
    if (ai) { /* the AI's old flow on the current timer */
        ok("clock.timer_delete", a); ok("clock.timer_delete", b);
        ok("clock.timer_set", "{\"seconds\":3}"); ok("clock.timer_start", NULL); int cur = (int)number("current_timer");
        CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 5000), "timer -> timer_set -> timer_start rings");
        snprintf(a, sizeof a, "{\"id\":%d}", cur); ok("clock.timer_dismiss", a);
        CHECK(WAIT_FOR(strcmp(active_state(), "alarm"), 5000), "dismissed by the AI");
        snprintf(a, sizeof a, "{\"seconds\":300,\"id\":%d}", cur); ok("clock.timer_set", a);
    }
}
static void tap_start(void) { press("开始"); }
static void tap_lap(void) { press("计次"); }
static void tap_pause(void) { press("暂停"); }
static void tap_sort(void) { press("单圈"); }
static void tap_city(void) { press("中国 · 香港"); }
static void tap_page(void) { key(INK_KEY_NEXT); }
static void tap_done(void) { press("完成"); }
static void tap_save(void) { press("保存"); }
static lv_point_t switch_at;
static void find_switch(void) {
    wanted = "7:00"; nth = 0; locate();
    if (hit) { lv_area_t r; lv_obj_get_coords(lv_obj_get_child(lv_obj_get_parent(hit), 0), &r); switch_at = (lv_point_t){(r.x1 + r.x2) / 2, (r.y1 + r.y2) / 2}; }
}
static void tap_switch(void) { on_ui(find_switch); CHECK(hit, "the alarm row's switch"); if (hit) tap(switch_at.x, switch_at.y); }
static void counts(void) { /* one panel refresh for each main action; the running stopwatch only touches its digits */
    open_clock(); ok("clock.stopwatch", NULL); ok("clock.stopwatch_reset", NULL);
    tab_alarm(); /* the first frame after opening the app can bring the panel's own second refresh: not counted */
    refreshes("tab switch", tab_watch); refreshes("tab switch to alarms", tab_alarm); refreshes("tab switch to timers", tab_timer); refreshes("tab switch to clock", tab_world); tab_alarm(); refreshes("alarm switch", tap_switch);
    CHECK(has("关闭"), "switched off"); refreshes("alarm switch back", tap_switch);
    tab_watch(); refreshes("start", tap_start);
    nallowed = 0; quiet(); allow_label(field("elapsed"), 0); watch_updates(10000);
    CHECK(updates >= 9 && updates <= 11 && outside == 0, "a running stopwatch, 10 s: only its digits, once a second (%u, %u outside)", updates, outside);
    refreshes("lap", tap_lap); refreshes("pause", tap_pause); refreshes("sort", tap_sort);
    tab_world(); press("加城市"); refreshes("pick a city", tap_city); refreshes("page turn", tap_page); refreshes("done", tap_done);
    ok("clock.world_remove", "{\"zone\":3}");
    tab_timer(); int t = (int)number("current_timer"); char a[32]; snprintf(a, sizeof a, "{\"id\":%d}", t);
    ok("clock.timer_edit", a); refreshes("counter plus", counter_plus); refreshes("save and back", tap_save);
}
static void members(void) { /* the child sees none of the adult's; the adult's timer rings while the child uses the device */
    open_clock(); ok("clock.timer", NULL); int t = timer_id("煮蛋"); char a[64];
    snprintf(a, sizeof a, "{\"seconds\":12,\"id\":%d}", t); ok("clock.timer_set", a); snprintf(a, sizeof a, "{\"id\":%d}", t); ok("clock.timer_start", a);
    CHECK(ink_member_switch("kid") == 0, "the child takes the device"); quiet();
    open_clock(); CHECK(has("\"tab\":\"alarm\"") && has("\"total\":0"), "the child starts on its own alarms, none of the adult's");
    if (!has("\"total\":0")) fprintf(stderr, "clock3 child: %s\n", strstr(snap, "\"tab\"") ? strstr(snap, "\"tab\"") : snap);
    ok("clock.timer", NULL); CHECK(has("\"seconds\":300") && !has("泡面") && !has("煮蛋"), "its own timer only");
    ok("clock.world", NULL); CHECK(!has("America/New_York"), "no adult cities");
    ok("clock.stopwatch", NULL); CHECK(number("elapsed_ms") == 0, "its own stopwatch");
    CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 12000) && has("煮蛋"), "the adult's timer rings for whoever holds the device");
    press("关闭"); CHECK(WAIT_FOR(strcmp(active_state(), "alarm"), 5000), "the child can close it");
    CHECK(ink_member_switch("owner") == 0, "back to the adult"); quiet();
    open_clock(); CHECK(has("\"tab\":\"timer\""), "the adult's last tab is the adult's");
}
static void restart(void) {
    open_clock(); CHECK(has("\"tab\":\"alarm\""), "after a restart the clock opens on the alarms");
    ok("clock.timer", NULL);
    CHECK(has("\"title\":\"泡面\",\"seconds\":65,\"remaining_seconds\":65,\"state\":\"idle\"") && has("\"title\":\"煮蛋\",\"seconds\":12,") &&
          !has("\"state\":\"running\""), "names and durations kept (煮蛋 was last set to 12 s), nothing running");
    CHECK(strcmp(active_state(), "alarm"), "no old timer rings");
    ok("clock.world", NULL); CHECK(has("\"label\":\"老妹家\""), "the renamed city kept");
    ok("clock.stopwatch", NULL); CHECK(number("elapsed_ms") == 0, "the stopwatch starts cleared");
}
static void shots(void) { /* the same pages at the largest text or as a child: everything fits */
    open_clock(); shot("Clock-alarm-empty");
    press("+ 加闹钟"); shot("ClockAlarm-new"); press("工作日"); ok("clock.save", NULL); shot("Clock-alarm");
    tap_switch(); press("7:00"); shot("ClockAlarm");
    ok("shell.back", NULL); tab_world(); shot("Clock-world-empty");
    press("加城市"); press("日本 · 东京"); shot("ClockWorldPick-chosen"); press("完成");
    ok("clock.world_add", "{\"zone\":7}"); ok("clock.world_add", "{\"zone\":0}"); shot("Clock-world");
    ok("clock.world_edit", "{\"zone\":7}"); shot("ClockCity"); ok("shell.back", NULL);
    tab_watch(); shot("Clock-stopwatch-empty"); press("开始");
    for (int i = 0; i < 3; i++) { wait_us(1100000 + 300000 * (uint64_t)i); press("计次"); }
    press("暂停"); shot("Clock-stopwatch");
    tab_timer(); shot("Clock-timer"); ok("clock.timer_new", NULL); ok("clock.timer_set", "{\"seconds\":3605}"); shot("ClockTimerEdit-new");
    type_name("clock.timer_name", "泡面"); ok("clock.timer_save", NULL);
    press("开始"); shot("Clock-timer-running");
    int t = timer_id("泡面"); char a[48]; snprintf(a, sizeof a, "{\"seconds\":2,\"id\":%d}", t); ok("clock.timer_set", a);
    snprintf(a, sizeof a, "{\"id\":%d}", t); ok("clock.timer_start", a);
    CHECK(WAIT_FOR(!strcmp(active_state(), "alarm"), 5000), "rings"); quiet(); shot("ClockTimerRing"); press("关闭");
    ok("clock.timer_delete", a); ok("clock.timer_delete", "{\"id\":0}");
    shot("Clock-timer-empty");
}
static void host_env(void *arg) {
    (void)arg; const char *p = getenv("INK_CLOCK3_PHASE"), *s = getenv("INK_CLOCK3_SUFFIX");
    snprintf(phase, sizeof phase, "%s", p ? p : "main"); snprintf(suffix, sizeof suffix, "%s", s ? s : "-normal");
}
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open() && settled(), 15000), "boot settles"); test_start_home(); ink_power_hold(INK_NEED_AWAKE);
    setting("time.auto", 0); setting("time.zone", 0);
    if (!strcmp(phase, "restart")) restart();
    else if (!strcmp(phase, "shots")) {
        set_time(utc(2026, 11, 20, 21, 0)); /* before the child's turn: children cannot set the clock */
        if (!strcmp(suffix, "-max")) setting("display.font_size", 3);
        if (!strcmp(suffix, "-child")) { CHECK(ink_member_switch("kid") == 0, "the child's set"); quiet(); }
        shots();
        ink_time_set(0, false); open_clock(); tab_world(); shot("Clock-world-notime"); /* the state before the time is known */
    } else {
        open_clock(); shot("Clock-alarm-empty");
        world(false); world(true); alarm(false); alarm(true); stopwatch(false); stopwatch(true); timer(false); timer(true);
        counts(); members();
    }
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "writes reach flash");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(host_env, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "clock3-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}

