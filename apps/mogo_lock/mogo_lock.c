/* Mogo's lock styles on our lock screen (lock-styles-1): one client of the lock style table. Each draw runs on the
 * background executor, the reader's own queue, for the member being drawn for; the reader's size comes back after. */
#include <runtime.h>
#include <lock_port.h>
#include <settings.h>
#include <ui_kit.h>
#include <book_ticket.h>
#include <book_lock_collage.h>
#include <book_progress.h>
#include <book_title.h>
#include <book_metadata.h>
#include <book_excerpt.h>
#include <book_cover.h>
#include <home_library.h>
#include <ui_calendar.h>
#include <ui_lock_clock.h>
#include <ui_mini_library.h>
#include <nvs.h>
#include "ink/osal.h"
#include "ink/sys/almanac.h"
#include "ink/sys/i18n.h"
#include "mogo_lock.h"

/* The one place that names how a book opens after the unlock (D-139 renames it: change it here only). */
#define MOGO_LOCK_OPEN_CAP "mogo.open"
enum { LIBRARY_Y = 420, LIBRARY_H = 472 };
static struct {
    char font_owner[16], collage_owner[16];
    /* the background executor's stack is 8 KB and Mogo's drawing takes most of it: the glue's words live here */
    char path[288], title[256], author[128], meta[256], line[INK_LOCK_SAYS_MAX / 2];
    struct { char path[288]; uint32_t when; } latest;
    book_mini_item_t books[BOOK_MINI_LIBRARY_MAX], own[BOOK_MINI_LIBRARY_MAX]; /* drawn, for a tap; the member's own */
    home_library_cover_t own_covers[BOOK_MINI_LIBRARY_MAX];
    unsigned count, selected;
    bool recent;
} L INK_PSRAM_BSS;

static void line(char *says, const char *label, const char *value) {
    size_t n = strlen(says);
    if (value && *value) snprintf(says + n, INK_LOCK_SAYS_MAX - n, "%s：%s\n", label, value);
}
static void text(uint8_t *fb, int x, int cy, int px, const char *s, enum EpdFontFlags align) {
    int above = 0, below = 0; px = ink_mogo_scaled(px);
    ttf_measure_line_px(px, s, &above, &below);
    ttf_draw_text_px(fb, x, cy + (above - below) / 2, px, s, align, 0, 15);
}
static void when(const ink_lock_request_t *r, char *time_text, size_t tcap, char *date_text, size_t dcap) {
    static const char *const weekdays[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
    if (!r->clock) { snprintf(time_text, tcap, "--:--"); snprintf(date_text, dcap, "%s", TR("请先校准时间")); return; }
    snprintf(time_text, tcap, "%02d:%02d", r->now.hour, r->now.min);
    snprintf(date_text, dcap, "%d月%d日 %s", r->now.month, r->now.day, weekdays[r->now.wday % 7]);
}
/* A member's books while their space is closed: the time and date stay, the books say they come after the unlock. */
static bool closed(uint8_t *fb, const ink_lock_request_t *r, ink_lock_drawn_t *out) {
    char t[16], d[48];
    when(r, t, sizeof t, d, sizeof d);
    text(fb, INK_LOCK_CANVAS_W / 2, 420, 120, t, EPD_DRAW_ALIGN_CENTER);
    text(fb, INK_LOCK_CANVAS_W / 2, 540, 32, d, EPD_DRAW_ALIGN_CENTER);
    text(fb, INK_LOCK_CANVAS_W / 2, 640, 30, TR("解锁后显示"), EPD_DRAW_ALIGN_CENTER);
    out->minute = (lv_area_t){0, 320, INK_LOCK_CANVAS_W - 1, 500};
    line(out->says, TR("读书内容"), TR("解锁后显示"));
    return true;
}
static bool font(const ink_lock_request_t *r, ink_lock_drawn_t *out) {
    if (ttf_font_ready() && (!L.font_owner[0] || !strcmp(L.font_owner, r->owner.member))) {
        snprintf(L.font_owner, sizeof L.font_owner, "%s", r->owner.member); return true;
    }
    if (r->open && ttf_font_open(app_settings_font_path()) == ESP_OK) {
        snprintf(L.font_owner, sizeof L.font_owner, "%s", r->owner.member); return true;
    }
    ttf_font_unload(); L.font_owner[0] = 0; /* no font file stays open while the lock says why (D-138) */
    snprintf(out->why, sizeof out->why, "%s", r->open ? TR("锁屏样式要用的字体还没放到卡上") : TR("解锁一次后才能显示这个锁屏样式"));
    return false;
}
static bool clock_style(uint8_t *fb, const ink_lock_request_t *r, ink_lock_drawn_t *out) {
    ui_lock_clock_face_t face = {.valid = r->clock, .hour = r->now.hour, .minute = r->now.min, .month = r->now.month,
                                 .day = r->now.day, .weekday = r->now.wday};
    ui_lock_clock_draw(fb, &face);
    EpdRect a = ui_lock_clock_area();
    out->minute = (lv_area_t){a.x, a.y, a.x + a.width - 1, a.y + a.height - 1};
    char t[16], d[48]; when(r, t, sizeof t, d, sizeof d);
    line(out->says, TR("时间"), t); line(out->says, TR("日期"), d);
    return true;
}
static bool calendar_style(uint8_t *fb, const ink_lock_request_t *r, ink_lock_drawn_t *out, bool monthly) {
    ink_almanac_t *a = ink_mogo_calloc(1, sizeof *a);
    calendar_date_t *d = ink_mogo_calloc(1, sizeof *d);
    bool ok = a && d && r->clock && ink_almanac_date(r->now.year, r->now.month, r->now.day, a);
    if (ok) { /* the time service's one almanac fills Mogo's own record, field by field */
#define COPY(f) d->f = a->f
#define LIST(f) memcpy(d->f, a->f, sizeof d->f)
        COPY(year); COPY(month); COPY(day); COPY(weekday); COPY(days); COPY(first_weekday); COPY(lunar_year); COPY(lunar_month);
        COPY(lunar_day); COPY(year_cycle); COPY(month_cycle); COPY(day_cycle); LIST(yi); LIST(ji); COPY(wealth); COPY(clash);
        COPY(sha); COPY(joy); COPY(fortune); LIST(auspicious); LIST(inauspicious); COPY(officer); COPY(guardian); COPY(guardian_type);
#undef LIST
#undef COPY
    }
    ui_calendar_draw(fb, ok ? d : NULL, monthly);
    if (ok) {
        char s[128];
        snprintf(s, sizeof s, "%d-%02d-%02d", a->year, a->month, a->day); line(out->says, TR("公历"), s);
        line(out->says, TR("农历"), a->lunar_text);
        snprintf(s, sizeof s, "%s %s %s", a->year_ganzhi, a->month_ganzhi, a->day_ganzhi); line(out->says, TR("干支"), s);
        if (!monthly || a->yi[0]) { line(out->says, TR("宜"), a->yi[0] ? a->yi : "无"); line(out->says, TR("忌"), a->ji[0] ? a->ji : "无"); }
        line(out->says, TR("冲"), a->clash); line(out->says, TR("煞"), a->sha);
    } else line(out->says, TR("日历"), r->clock ? TR("不在范围（2000—2099 年）") : TR("请先校准时间"));
    ink_app_free(a); ink_app_free(d);
    return true;
}
static void book_line(char *says, const char *path) {
    char s[64];
    L.author[0] = 0;
    if (!book_title_from_path(path, L.title, sizeof L.title)) return;
    if (book_metadata_supported(path)) (void)book_metadata_cached(path, L.meta, sizeof L.meta, L.author, sizeof L.author);
    line(says, TR("书名"), L.title); line(says, TR("作者"), L.author);
    struct stat st; book_progress_t p;
    if (!ink_mogo_lock_stat(path, &st) && book_progress_load(path, (uint32_t)st.st_size, &p)) {
        snprintf(s, sizeof s, "已读 %u%% · 第 %u 章", p.pct, (unsigned)p.chapter + 1); line(says, TR("进度"), s);
    }
    if (book_excerpt_load(path, L.line)) line(says, TR("书摘"), L.line);
}
/* Mogo's reader names the last book when it opens one (app_book.c:7867); ours does not yet (reader-2): until it does,
 * the book read most recently by the saved progress stands in, saved the way Mogo saves it. */
static bool latest_book(const char *path, const book_progress_t *p, void *ctx) {
    (void)ctx;
    if (p->last_open_s >= L.latest.when) { L.latest.when = p->last_open_s; snprintf(L.latest.path, sizeof L.latest.path, "%s", path); }
    return true;
}
static void last_book(void) {
    L.latest.path[0] = 0; L.latest.when = 0;
    if (book_progress_last_path(L.path, sizeof L.path) || book_progress_list(latest_book, NULL) != ESP_OK || !L.latest.path[0]) return;
    (void)book_progress_set_last_path(L.latest.path);
}
static bool ticket_style(uint8_t *fb, const ink_lock_request_t *r, ink_lock_drawn_t *out) {
    if (!r->open) return closed(fb, r, out);
    last_book();
    L.author[0] = 0;
    uint8_t *cover = book_progress_last_path(L.path, sizeof L.path) && book_title_from_path(L.path, L.title, sizeof L.title) ?
                     ink_mogo_malloc(BOOK_COVER_W * BOOK_COVER_H) : NULL;
    if (cover) { /* Mogo's shelf fills the cover cache the lock reads; here the lock fills it first, the same way */
        if (book_metadata_supported(L.path)) (void)book_metadata_cached(L.path, L.meta, sizeof L.meta, L.author, sizeof L.author);
        (void)book_cover_load_gray(L.path, L.title, L.author, cover, true, NULL);
        ink_app_free(cover);
    }
    if (!book_ticket_draw(fb, false) || !book_progress_last_path(L.path, sizeof L.path)) {
        snprintf(out->why, sizeof out->why, "%s", TR("还没有读过的书，读过一本后这里显示票根")); return false;
    }
    book_line(out->says, L.path);
    struct { uint32_t magic, day; uint64_t total_seconds, today_seconds; uint32_t total_turns, today_turns; } stats = {0};
    nvs_handle_t h; size_t n = sizeof stats; /* book_ticket.c's own record (ticket_stats_t), read for the snapshot */
    if (!nvs_open("rp_ticket", NVS_READONLY, &h)) { if (nvs_get_blob(h, "stats", &stats, &n) || n != sizeof stats) stats.day = 0; nvs_close(h); }
    time_t now = ink_mogo_lock_time(NULL);
    char s[64];
    if (now >= 1704067200 && stats.day == (uint32_t)((now + 8 * 3600) / 86400)) snprintf(s, sizeof s, "%llu 分钟", (unsigned long long)(stats.today_seconds / 60));
    else snprintf(s, sizeof s, "%s", "—");
    line(out->says, TR("今日阅读"), s);
    return true;
}
static void titles(char *says, const book_mini_item_t *books, unsigned count) {
    L.line[0] = 0;
    for (unsigned i = 0; i < count; i++) {
        size_t n = strlen(L.line);
        snprintf(L.line + n, sizeof L.line - n, "%s%u. %s（%s）", i ? "；" : "", i + 1, books[i].title, books[i].path);
    }
    line(says, TR("书"), L.line);
}
/* Only the drawn-for member's own books: Mogo's model is one person's, so whatever it still holds from someone else
 * (a scan that stopped early keeps the last one) is dropped unless the file is in this member's space. */
static home_library_view_t own(home_library_view_t v) {
    unsigned n = 0, selected = 0;
    struct stat st;
    for (unsigned i = 0; i < v.count && i < BOOK_MINI_LIBRARY_MAX; i++) {
        if (strncmp(v.books[i].path, "/me/", 4) || ink_mogo_lock_stat(v.books[i].path, &st)) continue;
        if (i == v.selected) selected = n;
        L.own[n] = v.books[i];
        L.own_covers[n++] = v.covers ? v.covers[i] : (home_library_cover_t){0};
    }
    v.books = L.own; v.covers = L.own_covers; v.count = n; v.selected = selected;
    return v;
}
static bool library_load(const ink_lock_request_t *r) {
    home_library_invalidate(); /* scanned afresh for whoever it draws for: no one else's books stay in the model */
    home_library_enter();
    if (!home_library_poll(NULL, NULL)) { home_library_leave(); return false; }
    home_library_cover_generation(1);
    for (int i = 0; i < BOOK_MINI_LIBRARY_MAX && home_library_poll(NULL, NULL); i++) {}
    home_library_select((unsigned)r->select);
    return true;
}
static bool collage_style(uint8_t *fb, const ink_lock_request_t *r, ink_lock_drawn_t *out) {
    if (!r->open) return closed(fb, r, out);
    if (strcmp(L.collage_owner, r->owner.member)) book_lock_collage_invalidate(); /* the catalogue index was someone else's */
    snprintf(L.collage_owner, sizeof L.collage_owner, "%s", r->owner.member);
    if (!book_lock_collage_prepare(fb)) { snprintf(out->why, sizeof out->why, "%s", TR("书架拼贴没画出来，先显示默认锁屏")); return false; }
    if (library_load(r)) { /* the recent books, for the snapshot: the collage puts the most recent first */
        home_library_view_t v = own(home_library_view());
        titles(out->says, v.books, v.count);
        home_library_leave();
    }
    return true;
}
static bool library_style(uint8_t *fb, const ink_lock_request_t *r, ink_lock_drawn_t *out) {
    if (!r->open) { L.count = 0; return closed(fb, r, out); }
    char t[16], d[48]; when(r, t, sizeof t, d, sizeof d);
    text(fb, INK_LOCK_CANVAS_W / 2, 230, 96, t, EPD_DRAW_ALIGN_CENTER);
    text(fb, INK_LOCK_CANVAS_W / 2, 330, 30, d, EPD_DRAW_ALIGN_CENTER);
    out->minute = (lv_area_t){0, 150, INK_LOCK_CANVAS_W - 1, 300};
    line(out->says, TR("时间"), t); line(out->says, TR("日期"), d);
    if (!library_load(r)) { snprintf(out->why, sizeof out->why, "%s", TR("书库暂不可用，请稍后重试")); return false; }
    home_library_view_t v = own(home_library_view());
    v.loading = false;
    ui_mini_library_draw(fb, (EpdRect){0, LIBRARY_Y, INK_LOCK_CANVAS_W, LIBRARY_H}, &v, (ui_mini_library_frame_t){v.selected, 256, 256, false});
    if (!v.recent) epd_fill_rect((EpdRect){526, LIBRARY_Y + 408, 134, 44}, 255, fb); /* no shuffle on the lock screen */
    L.count = v.count; L.selected = v.selected; L.recent = v.recent;
    memcpy(L.books, v.books, v.count * sizeof *v.books);
    line(out->says, TR("迷你书库"), v.recent ? TR("最近") : TR("随机"));
    titles(out->says, v.books, v.count);
    if (!v.count) line(out->says, TR("书"), v.recent ? "还没有阅读记录" : "书匣等你放入第一本书");
    home_library_leave();
    return true;
}
static bool draw(const ink_lock_request_t *r, uint8_t *canvas, ink_lock_drawn_t *out) {
    ink_member_identity_t page_owner = *ink_mogo_current_owner();
    int w, h, px; ink_mogo_dimensions_get(&w, &h, &px);
    ink_mogo_owner(&r->owner); ink_mogo_dimensions(INK_LOCK_CANVAS_W, INK_LOCK_CANVAS_H, r->px);
    ink_mogo_lock_request = r;
    bool ok = font(r, out);
    if (ok) switch (r->style) {
        case INK_LOCK_TICKET: ok = ticket_style(canvas, r, out); break;
        case INK_LOCK_COLLAGE: ok = collage_style(canvas, r, out); break;
        case INK_LOCK_DIGITAL: ok = clock_style(canvas, r, out); break;
        case INK_LOCK_ALMANAC: ok = calendar_style(canvas, r, out, false); break;
        case INK_LOCK_PERPETUAL: ok = calendar_style(canvas, r, out, true); break;
        case INK_LOCK_LIBRARY: ok = library_style(canvas, r, out); break;
        default: ok = false;
    }
    ink_mogo_lock_request = NULL;
    ink_mogo_owner(&page_owner); ink_mogo_dimensions(w, h, px);
    return ok;
}
static ink_lock_tap_t tap(int style, int x, int y, int *index, char *cap, size_t cap_size, char *args, size_t args_size) {
    if (style != INK_LOCK_LIBRARY || !L.count) return INK_LOCK_TAP_NONE;
    home_library_view_t v = {.books = L.books, .count = L.count, .selected = L.selected, .recent = L.recent};
    ui_mini_hit_t hit = ui_mini_library_hit((EpdRect){0, LIBRARY_Y, INK_LOCK_CANVAS_W, LIBRARY_H}, &v, x, y);
    if (hit.index >= L.count) return INK_LOCK_TAP_NONE;
    if (hit.action == UI_MINI_SELECT) { *index = (int)hit.index; return INK_LOCK_TAP_SELECT; }
    if (hit.action != UI_MINI_OPEN) return INK_LOCK_TAP_NONE;
    snprintf(cap, cap_size, "%s", MOGO_LOCK_OPEN_CAP);
    snprintf(args, args_size, "{\"path\":\"%s\"}", L.books[hit.index].path);
    return INK_LOCK_TAP_OPEN;
}
static void shown(int style, const uint8_t *canvas) { if (style == INK_LOCK_COLLAGE) book_lock_collage_cache_commit(canvas); }
static uint32_t revision(int style) { return style == INK_LOCK_TICKET || style == INK_LOCK_COLLAGE || style == INK_LOCK_LIBRARY ? book_progress_revision() : 0; }
static const ink_lock_client_t CLIENT = {.name = "mogo_lock", .first = INK_LOCK_TICKET, .count = INK_LOCK_LIBRARY - INK_LOCK_TICKET + 1,
    .draw = draw, .tap = tap, .shown = shown, .revision = revision};
_Static_assert(sizeof "mogo_lock" <= INK_LOCK_NAME_MAX, "client name fits the table");
int ink_mogo_lock_init(void) { return ink_lock_face_register(&CLIENT); }
