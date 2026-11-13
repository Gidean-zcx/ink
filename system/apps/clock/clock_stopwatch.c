/* The stopwatch tab after Fossify Clock (对照表 F): InfiniTime's controller, one per member, never saved; laps show their
 * own time and the total and sort by a column. Whole seconds while it runs, hundredths when paused. UI task only. */
/* ui-scale-runtime: dimension w,x,y,h,bw,row,per,bar,sw Theme-derived coordinates supplied by this page. */
#include "clock_internal.h"
#include <stdlib.h>

enum { BY_LAP, BY_LAP_TIME, BY_TOTAL, ASCENDING = 4 }; /* ck_member_t.sort; zero is Fossify's default, lap number descending */
static const char *const SORT_ID[] = {"lap", "lap_time", "total"};
static struct { lv_obj_t *digits; int field, member; uint8_t order[CK_LAPS]; unsigned first, shown; } S INK_PSRAM_BSS;

static void format(char *out, size_t cap, uint32_t ms, bool hundredths) { /* 00:12.34; 1:02:50.50 past an hour */
    uint32_t s = ms / 1000;
    if (s >= 3600) snprintf(out, cap, "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
    else snprintf(out, cap, "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
    size_t n = strlen(out);
    if (hundredths) snprintf(out + n, cap - n, ".%02u", (unsigned)(ms / 10 % 100));
}
static uint32_t total_of(int lap) { return lap > 0 ? ck_watch_lap_ms(S.member, lap) : 0; }
static uint32_t key(int lap, int by) { return by == BY_LAP ? (uint32_t)lap : by == BY_TOTAL ? total_of(lap) : total_of(lap) - total_of(lap - 1); }
static int compare(const void *a, const void *b) {
    int by = ck_me->sort & 3; uint32_t x = key(*(const uint8_t *)a + 1, by), y = key(*(const uint8_t *)b + 1, by);
    int r = x < y ? -1 : x > y;
    return ck_me->sort & ASCENDING ? r : -r;
}
bool ck_watch_running(void) { return ck_watch_state(ck_member()) == 1; }
void ck_watch_body(int h) {
    lv_obj_t *b = ck_body; int w = INK_CANVAS_W - 2 * INK_MARGIN, y = 0, m = S.member = ck_member(), state = ck_watch_state(m);
    uint32_t ms = ck_watch_ms(m); char t[24];
    format(t, sizeof t, ms, state == 2);
    int sw = ink_ui_text_width(TR("已暂停"), INK_FONT_NOTE, INK_WEIGHT_REGULAR);
    S.digits = ink_ui_line(b, t, INK_FONT_NUMBER, INK_WEIGHT_REGULAR, INK_MARGIN, y, w - sw - INK_GAP);
    S.field = ck_field("elapsed", TR("秒表"), t);
    if (state == 2) { /* paused: said in words, nothing blinks */
        lv_obj_t *l = ink_ui_line(b, TR("已暂停"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + w - sw, y + INK_LINE_NUMBER - ink_ui_line_height(INK_FONT_NOTE) - INK_GAP_S, sw);
        lv_obj_set_style_text_color(l, ink_ui_secondary(), 0); ck_field("state", TR("状态"), TR("已暂停"));
    }
    y += INK_LINE_NUMBER + INK_GAP_XS;
    int bw = (w - 2 * INK_GAP_M) / 3, laps = ck_watch_laps(m);
    ck_btn(b, state == 1 ? TR("暂停") : state ? TR("继续") : TR("开始"), state == 1 ? "clock.stopwatch_pause" : "clock.stopwatch_start", NULL, INK_MARGIN, y, bw, INK_CONTROL, true, true);
    ck_btn(b, TR("计次"), "clock.stopwatch_lap", NULL, INK_MARGIN + bw + INK_GAP_M, y, bw, INK_CONTROL, false, state == 1 && laps < CK_LAPS);
    ck_btn(b, TR("清零"), "clock.stopwatch_reset", NULL, INK_MARGIN + 2 * (bw + INK_GAP_M), y, bw, INK_CONTROL, false, state != 0);
    y += INK_CONTROL + INK_GAP;
    bool open = state == 2 && laps && ms > total_of(laps); /* the lap not yet taken: first while paused */
    unsigned count = laps ? (unsigned)laps + open : 0;
    if (!count) { ck_pager(b, 0, &ck_me->page[TAB_WATCH], 0, 1); return; }
    static const char *const COLUMN[] = {TR_NOOP("圈次"), TR_NOOP("单圈"), TR_NOOP("累计")};
    for (int i = 0; i < 3; i++)
        ck_btn(b, ck_str("%s%s", TR(COLUMN[i]), (ck_me->sort & 3) != i ? "" : ck_me->sort & ASCENDING ? " ↑" : " ↓"), "clock.lap_sort",
               ck_str("{\"by\":\"%s\"}", SORT_ID[i]), INK_MARGIN + i * (bw + INK_GAP_M), y, bw, INK_CONTROL, false, true);
    y += INK_CONTROL + INK_GAP_XS;
    int nw = ink_ui_text_width(ck_str(TR("第 %d 圈（未完）"), 99), INK_FONT_NOTE, INK_WEIGHT_REGULAR), line = ink_ui_line_height(INK_FONT_NOTE);
    int vw = ink_ui_text_width(ck_str(TR("单圈 %s · 累计 %s"), "00:00.00", "00:00.00"), INK_FONT_NOTE, INK_WEIGHT_REGULAR);
    bool inline_times = nw + INK_GAP + vw <= w; /* the lap and its times on one line, or the times under it (largest text) */
    int row = (inline_times ? 1 : 2) * line + INK_GAP_XS, bar = h - INK_MARGIN - INK_CONTROL;
    int per = (h - INK_MARGIN - y) / row;
    if ((int)count > per) per = LV_MAX(1, (bar - INK_GAP_M - y) / row);
    for (int i = 0; i < laps; i++) S.order[i] = (uint8_t)i;
    qsort(S.order, (size_t)laps, 1, compare);
    S.first = ck_pager(b, bar, &ck_me->page[TAB_WATCH], count, (unsigned)per); S.shown = 0;
    for (unsigned k = S.first; k < count && (int)(k - S.first) < per; k++) {
        bool live = open && k == 0; int lap = live ? laps + 1 : S.order[k - open] + 1;
        uint32_t total = live ? ms : total_of(lap); char one[24], all[24];
        format(one, sizeof one, total - total_of(lap - 1), true); format(all, sizeof all, total, true);
        const char *name = live ? ck_str(TR("第 %d 圈（未完）"), lap) : ck_str(TR("第 %d 圈"), lap), *value = ck_str(TR("单圈 %s · 累计 %s"), one, all);
        int at = y + (int)(k - S.first) * row;
        ink_ui_line(b, name, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, at, inline_times ? nw : w);
        lv_obj_t *v = ink_ui_line(b, value, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + (inline_times ? w - vw : 0), at + (inline_times ? 0 : line), vw);
        if (inline_times) lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
        ck_field(name, name, value); S.shown++;
    }
}
bool ck_watch_tick(void) { /* whole seconds while running: only the digits, only when they change */
    if (ck_watch_state(S.member) != 1 || !S.digits) return false;
    char t[24]; format(t, sizeof t, ck_watch_ms(S.member), false);
    ck_text(S.digits, t, true); ck_value(S.field, t);
    return true;
}
void ck_watch_extra(ink_jw_t *w) {
    uint32_t ms = ck_watch_ms(S.member); int laps = ck_watch_laps(S.member); char s[160];
    snprintf(s, sizeof s, ",\"elapsed_ms\":%u,\"elapsed_seconds\":%u,\"running\":%s,\"sort\":\"%s_%s\",\"laps\":[", (unsigned)ms, (unsigned)(ms / 1000),
             ck_watch_state(S.member) == 1 ? "true" : "false", SORT_ID[ck_me->sort & 3], ck_me->sort & ASCENDING ? "asc" : "desc");
    ink_jw_raw(w, s);
    for (unsigned i = 0, n = 0; i < S.shown; i++) {
        unsigned k = S.first + i; bool live = ck_watch_state(S.member) == 2 && laps && ms > total_of(laps);
        if (live && k == 0) continue;
        int lap = S.order[k - live] + 1;
        snprintf(s, sizeof s, "%s{\"lap\":%d,\"lap_ms\":%u,\"total_ms\":%u}", n++ ? "," : "", lap, (unsigned)(total_of(lap) - total_of(lap - 1)), (unsigned)total_of(lap));
        ink_jw_raw(w, s);
    }
    ink_jw_raw(w, "]");
}
void ck_watch_cap(ink_call_t *c) {
    const char *id = c->cap->id + 6; int m = ck_member();
    if (m < 0 || !ck_on("Clock") || ck_me->tab != TAB_WATCH) { c->rc.status = INK_RC_STALE; return; }
    if (!strcmp(id, "stopwatch_start")) ck_watch_start(m);
    else if (!strcmp(id, "stopwatch_pause")) ck_watch_pause(m);
    else if (!strcmp(id, "stopwatch_reset")) { ck_watch_clear(m); ck_me->page[TAB_WATCH] = 0; }
    else if (!strcmp(id, "stopwatch_lap")) {
        if (!ck_watch_lap(m)) { c->rc.status = INK_RC_REJECTED; return; } /* not running, or the 100th */
        ck_me->page[TAB_WATCH] = 0; /* back to the top, where the new lap is */
    } else {
        char by[12] = ""; ink_args_get_str(&c->args, "by", by, sizeof by);
        int col = !strcmp(by, "lap_time") ? BY_LAP_TIME : !strcmp(by, "total") ? BY_TOTAL : BY_LAP;
        ck_me->sort = (uint8_t)((ck_me->sort & 3) == col ? ck_me->sort ^ ASCENDING : col); /* a new column starts descending */
        ck_me->page[TAB_WATCH] = 0;
    }
    ck_refresh();
}
