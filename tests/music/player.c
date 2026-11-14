/* music-3 acceptance on the whole product: the kiikoread walk by touch and again by AI (same results, same snapshots),
 * playing on in the background, members and resuming, the old music data, no rebinding, what each action refreshes,
 * what is missing, and the pictures. Phases (INK_MUSIC_PHASE) share one flash image; see run.py. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#include "ink/sys/audio.h"
#include "ink/sys/crc.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/media.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include "../../apps/music/port/music_page.h"
#include "graft.h"

#define MINE "/me/音乐/"
#define FIRST "/family/audio/0首.mp3" /* tagged, about 21 s, with its own lyrics */
#define WRAP FIRST
static char phase[16], content[8], snap[12000], value[512], walk_log[2][16][1024];
static EpdRect T[48];
static int nt, top;
static volatile bool listed;
static ink_exec_job_t list_job;
static void read_env(void *arg) {
    (void)arg;
    snprintf(phase, sizeof phase, "%s", getenv("INK_MUSIC_PHASE") ? getenv("INK_MUSIC_PHASE") : "main");
    snprintf(content, sizeof content, "%s", getenv("INK_MUSIC_CONTENT") ? getenv("INK_MUSIC_CONTENT") : "full");
}
static ink_media_status_t status(void) { ink_media_status_t s; ink_media_status(&s); return s; }
static volatile bool bg_seen;
static ink_exec_job_t barrier_job;
static void bg_mark(void *arg) { (void)arg; bg_seen = true; }
static void ui_noop(void) {}
static void settle(void) { /* what the page drew for the last action is on its way: no waiting for the panel */
    for (int i = 0; i < 2; i++) { bg_seen = false; ink_exec_post(INK_EXEC_BG, &barrier_job, bg_mark, NULL); CHECK(WAIT_FOR(bg_seen, 8000), "page work done"); on_ui(ui_noop); }
}
static void quiet(void) { /* the page's executor, then the UI task that presents, three times over */
    for (int i = 0; i < 3; i++) { bg_seen = false; ink_exec_post(INK_EXEC_BG, &barrier_job, bg_mark, NULL); CHECK(WAIT_FOR(bg_seen, 8000), "page work done"); on_ui(ui_noop); }
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "display settles");
}
static void ok(const char *cap, const char *args) { CHECK(AI(cap, args) && g_rc.status == INK_RC_DONE, "%s %.80s: %s %.100s", cap, args ? args : "", ink_rc_name(g_rc.status), g_rc.detail); }
static void grab(void) { ink_graft_snapshot(snap, sizeof snap); }
static const char *field(const char *id) { /* a snapshot field's value ("" when absent) */
    char key[48]; snprintf(key, sizeof key, "{\"id\":\"%s\"", id);
    const char *p = strstr(snap, key), *v = p ? strstr(p, "\"value\":\"") : NULL, *e = v ? strchr(v + 9, '"') : NULL;
    snprintf(value, sizeof value, "%.*s", e ? (int)(e - v - 9) : 0, e ? v + 9 : "");
    return value;
}
static bool shows(const char *id, const char *text) { on_ui(grab); return strstr(field(id), text) != NULL; }
static const char *label(const char *id) {
    char key[48]; snprintf(key, sizeof key, "{\"id\":\"%s\"", id);
    const char *p = strstr(snap, key), *v = p ? strstr(p, "\"label\":\"") : NULL, *e = v ? strchr(v + 9, '"') : NULL;
    snprintf(value, sizeof value, "%.*s", e ? (int)(e - v - 9) : 0, e ? v + 9 : "");
    return value;
}
static int row_of(const char *path);
static bool row_favorite(const char *path) { char id[12]; int r = row_of(path); snprintf(id, sizeof id, "track%d", r); return r >= 0 && strstr(label(id), "已收藏"); }
static int row_of(const char *path) { /* the row on screen holding `path`, or -1 */
    on_ui(grab);
    for (int i = 0; i < 12; i++) { char id[12]; snprintf(id, sizeof id, "track%d", i); if (!strcmp(field(id), path)) return i; }
    return -1;
}
static void debug_picture(void);
static void read_targets(void *arg) { (void)arg; nt = music_page_targets(T, 48); listed = true; }
static void targets(void) {
    quiet(); listed = false; ink_exec_post(INK_EXEC_BG, &list_job, read_targets, NULL); CHECK(WAIT_FOR(listed, 3000), "page hit table");
    for (int i = 0; i < nt; i++) {
        CHECK(T[i].width >= ink_ui_px(INK_CONTROL) && T[i].height >= ink_ui_px(INK_CONTROL), "target %d at least 108 px (%dx%d)", i, T[i].width, T[i].height);
        CHECK(T[i].x >= 0 && T[i].y >= 0 && T[i].x + T[i].width <= ink_ui_width() && top + T[i].y + T[i].height <= ink_ui_height(), "target %d on screen", i);
    }
}
/* Player view, in hit-table order: all, output, mode, bar, prev, play, next, minus, plus, rows..., page prev, page next. */
enum { ALL, OUTPUT, MODE, BAR, PREV, PLAY, NEXT, MINUS, PLUS, ROW0 };
static void touch(int x, int y) {
    double before = sim_metric_value("music.touch_call");
    tap(x, top + y);
    bool reached = WAIT_FOR(sim_metric_value("music.touch_call") > before, 2000);
    static int shots; if (!reached && !shots++) { debug_picture(); }
    CHECK(reached, "tap reaches a capability at %d,%d", x, y); quiet();
}
static void press(int i) { targets(); touch(T[i].x + T[i].width / 2, T[i].y + T[i].height / 2); }
static unsigned bar_ms(int rel) { return (unsigned)((uint64_t)status().duration_ms * (unsigned)rel / (unsigned)T[BAR].width); }
static void seek_to(bool by_touch, int rel, bool check) { /* rel: pixels into the bar */
    if (by_touch) targets();
    unsigned ms = bar_ms(rel);
    if (by_touch) touch(T[BAR].x + rel, T[BAR].y + T[BAR].height / 2);
    else { char a[32]; snprintf(a, sizeof a, "{\"ms\":%u}", ms); ok("music.seek", a); }
    if (check) CHECK(WAIT_FOR(status().position_ms >= ms && status().position_ms <= ms + 1500, 3000), "seek lands at %u (%u)", ms, (unsigned)status().position_ms);
}
static void seek(bool by_touch, int rel) { seek_to(by_touch, rel, true); }
static void near_end(bool by_touch) { if (by_touch || !nt) targets(); seek_to(by_touch, T[BAR].width - 2, false); }
static void page_to(bool by_touch, const char *path, int page) { /* the playlist page holding path */
    for (int i = 0; by_touch && i < 8 && row_of(path) < 0; i++) press(atoi(field("page") + strlen("第 ")) - 1 < page ? nt - 1 : nt - 2);
    if (by_touch) {}
    else { char a[48]; snprintf(a, sizeof a, "{\"view\":\"list\",\"page\":%d}", page); ok("music.list", a); quiet(); }
    CHECK(row_of(path) >= 0, "row for %s", path);
}
static void row_tap(bool by_touch, const char *path, int page, bool favorite) {
    page_to(by_touch, path, page);
    if (by_touch) { targets(); EpdRect r = T[ROW0 + row_of(path)]; touch(favorite ? r.x + ink_ui_px(INK_CONTROL) / 2 : r.x + r.width / 2, r.y + r.height / 2); }
    else { char a[160]; snprintf(a, sizeof a, "{\"path\":\"%s\"}", path); ok(favorite ? "music.favorite" : "music.play", a); }
    if (!favorite) CHECK(WAIT_FOR(!strcmp(status().path, path) && status().state == INK_MEDIA_PLAYING, 3000), "plays %s (%s, state %d, page %s)", path, status().path, status().state, (on_ui(grab), field("page")));
}
static void mode(bool by_touch, const char *label, const char *name) {
    if (by_touch) { for (int i = 0; i < 4 && !shows("mode", label); i++) press(MODE); }
    else { char a[48]; snprintf(a, sizeof a, "{\"mode\":\"%s\"}", name); ok("music.mode", a); quiet(); }
    CHECK(shows("mode", label), "mode %s", label);
}
static void finishes_into(const char *next) { CHECK(WAIT_FOR(!strcmp(status().path, next) && status().state == INK_MEDIA_PLAYING, 8000), "next is %s (%s)", next, status().path); }
static void checkpoint(int run, int at) { /* everything the snapshot says except where the song is */
    quiet(); on_ui(grab);
    char *out = walk_log[run][at]; size_t n = 0;
    const bool playing = !strcmp(field("state"), "正在放"); /* the lyric then depends on how far it played */
    const char *ids[] = {"title", "artist", "state", "mode", "lyrics", "favorite", "page", "library", "track0", "track1", "track2", "track3", "track4", "track5", "track6"};
    for (size_t i = 0; i < sizeof ids / sizeof *ids && n < sizeof walk_log[0][0]; i++) {
        if (playing && !strcmp(ids[i], "lyrics")) continue;
        n += (size_t)snprintf(out + n, sizeof walk_log[0][0] - n, "%s=%s|", ids[i], field(ids[i]));
        if (n < sizeof walk_log[0][0]) n += (size_t)snprintf(out + n, sizeof walk_log[0][0] - n, "%s;", label(ids[i]));
    }
}
static void walk(bool by_touch) {
    const int run = by_touch ? 0 : 1;
    ok("music.mode", "{\"mode\":\"list\"}");
    for (int guard = 0; guard < 8 && AI("music.favorites", NULL) && strstr(g_result, "\"path\":\""); guard++) {
        char path[160]; const char *p = strstr(g_result, "\"path\":\"") + 8; snprintf(path, sizeof path, "%.*s", (int)(strchr(p, '"') - p), p);
        char a[200]; snprintf(a, sizeof a, "{\"path\":\"%s\"}", path); ok("music.favorite", a);
    }
    ok("music.list", "{\"view\":\"player\",\"page\":0}"); quiet();
    /* Title from the tags; play, the lyric line follows the position; the bar seeks. */
    row_tap(by_touch, FIRST, 0, false);
    CHECK(shows("title", "合成音调"), "title is the tagged name");
    if (by_touch) press(PLAY); else ok("music.pause", NULL);
    CHECK(WAIT_FOR(status().state == INK_MEDIA_PAUSED, 2000), "paused");
    targets(); seek(by_touch, T[BAR].width / 2); CHECK(shows("lyrics", "第二句"), "lyric at the middle: %s", field("lyrics")); checkpoint(run, 0);
    targets(); seek(by_touch, T[BAR].width * 5 / 6); CHECK(shows("lyrics", "第三句"), "the lyric changes"); checkpoint(run, 1);
    /* Favorite one song in its row, switch to the favorites loop: the end of another song goes to it. */
    row_tap(by_touch, MINE "乙.wav", 4, true); CHECK(row_favorite(MINE "乙.wav"), "row favorite"); checkpoint(run, 2);
    mode(by_touch, "收藏循环", "favorites"); checkpoint(run, 3);
    row_tap(by_touch, MINE "甲.wav", 5, false); near_end(by_touch); finishes_into(MINE "乙.wav"); checkpoint(run, 4);
    /* Single repeat plays it again. */
    mode(by_touch, "单曲循环", "one");
    double started = sim_metric_value("media.started");
    near_end(by_touch);
    CHECK(WAIT_FOR(sim_metric_value("media.started") > started && !strcmp(status().path, MINE "乙.wav"), 8000), "single repeat replays");
    /* The list loop goes in order and wraps to the first. */
    mode(by_touch, "列表循环", "list");
    row_tap(by_touch, MINE "甲.wav", 5, false); near_end(by_touch); finishes_into(MINE "长歌.wav");
    near_end(by_touch); finishes_into(WRAP); checkpoint(run, 5);
    /* Shuffle never picks the song that just played. */
    mode(by_touch, "随机播放", "shuffle");
    for (int i = 0; i < 20; i++) {
        char was[128]; snprintf(was, sizeof was, "%s", status().path);
        near_end(false);
        CHECK(WAIT_FOR(strcmp(status().path, was) && status().state == INK_MEDIA_PLAYING, 8000), "shuffle %d leaves %s", i, was);
    }
    ok("music.list", "{\"view\":\"player\"}"); quiet();
    /* With the favorites cleared the favorites loop falls back to the list. */
    mode(by_touch, "收藏循环", "favorites");
    row_tap(by_touch, MINE "乙.wav", 4, true); CHECK(!row_favorite(MINE "乙.wav"), "favorite cleared");
    row_tap(by_touch, MINE "丙.wav", 4, false); near_end(by_touch); finishes_into(MINE "乙.wav"); checkpoint(run, 6);
    /* Manual next/previous walk the whole library, even with a favorite elsewhere. */
    row_tap(by_touch, MINE "甲.wav", 5, true);
    row_tap(by_touch, MINE "丙.wav", 4, false);
    if (by_touch) press(NEXT); else ok("music.next", NULL);
    CHECK(WAIT_FOR(!strcmp(status().path, MINE "乙.wav"), 3000), "next is the next in the library, not the favorite");
    if (by_touch) press(PREV); else ok("music.prev", NULL);
    CHECK(WAIT_FOR(!strcmp(status().path, MINE "丙.wav"), 3000), "previous goes back"); checkpoint(run, 7);
    if (by_touch) press(PLAY); else ok("music.pause", NULL);
    CHECK(WAIT_FOR(status().state == INK_MEDIA_PAUSED, 2000), "paused for browsing");
    /* All songs by folder: in, up, the next page, back to the player. */
    if (by_touch) press(ALL); else ok("music.list", "{\"view\":\"all\"}");
    quiet(); on_ui(grab); CHECK(!strcmp(label("page"), "全部歌曲") && shows("page", "第 2 页"), "all songs open on the page of the playing song: %s", field("page"));
    if (by_touch) press(nt - 2); else ok("music.list", "{\"page\":0}");
    quiet(); CHECK(shows("page", "第 1 页") && strstr(snap, "\"directory\""), "folders at the root"); checkpoint(run, 8);
    int folder = -1;
    for (int i = 0; i < 12 && folder < 0; i++) { char id[12]; snprintf(id, sizeof id, "track%d", i); on_ui(grab); if (!strcmp(field(id), "文件夹")) folder = i; }
    CHECK(folder >= 0, "folder row");
    if (by_touch) { targets(); touch(T[1 + folder].x + T[1 + folder].width / 2, T[1 + folder].y + T[1 + folder].height / 2); }
    else ok("music.list", "{\"path\":\"/family/audio/文件夹\"}");
    quiet(); CHECK(row_of("/family/audio/文件夹/独立曲目.flac") >= 0, "inside the folder"); checkpoint(run, 9);
    if (by_touch) press(0); else ok("music.list", "{\"path\":\"/\"}");
    quiet(); CHECK(shows("track0", "专辑") || shows("track0", "文件夹"), "back at the root"); checkpoint(run, 10);
    if (by_touch) { targets(); touch(T[nt - 1].x + T[nt - 1].width / 2, T[nt - 1].y + T[nt - 1].height / 2); } else ok("music.list", "{\"page\":1}");
    quiet(); CHECK(shows("page", "第 2 页"), "next page of all songs"); checkpoint(run, 11);
    if (by_touch) press(0); else ok("music.list", "{\"view\":\"player\"}");
    quiet(); on_ui(grab); CHECK(!strcmp(label("page"), "播放列表"), "back on the player"); checkpoint(run, 12);
}
static int callbacks(lv_obj_t *o) {
    int n = (int)lv_obj_get_event_count(o);
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) n += callbacks(lv_obj_get_child(o, (int32_t)i));
    return n;
}
static int counted;
static void count_callbacks(void) { counted = callbacks(lv_screen_active()); }
static uint32_t raster;
static const uint8_t *raster_px; /* the page as the panel has it (the graft surface) */
static int raster_w;
static void find_raster(lv_obj_t *o) {
    if (lv_obj_check_type(o, &lv_image_class)) {
        const lv_image_dsc_t *d = lv_image_get_src(o);
        if (d && d->header.cf == LV_COLOR_FORMAT_L8) raster = ink_crc32(0, d->data, d->data_size), raster_px = d->data, raster_w = (int)d->header.w;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_raster(lv_obj_get_child(o, (int32_t)i));
}
static void pixels(void) { raster = 0; raster_px = NULL; find_raster(lv_screen_active()); }
static int band[4], band_dark; /* x0, x1, y0, y1 in surface pixels; how many inked pixels the panel has there */
static bool bar_whole;         /* the bar row on the panel: black then gray, never gray then black again */
static void read_band(void) {
    pixels(); band_dark = 0;
    for (int y = band[2]; raster_px && y < band[3]; y++)
        for (int x = band[0]; x < band[1]; x++) band_dark += raster_px[y * raster_w + x] < 224; /* any ink, gray text too */
    bool gray = false; bar_whole = raster_px != NULL;
    for (int x = band[0]; raster_px && x < band[1]; x++) {
        const bool black = raster_px[band[2] * raster_w + x] < 96;
        if (black && gray) bar_whole = false;
        gray |= !black;
    }
}
static int dark_in(int x0, int x1, int y0, int y1) { band[0] = x0; band[1] = x1; band[2] = y0; band[3] = y1; quiet(); on_ui(read_band); return band_dark; }
static lv_obj_t *panel;
static void ai_open(void) { lv_obj_t *a, *b, *c; ink_shell_ai_open(&panel, &a, &b, &c); }
static void ai_close(void) { ink_shell_ai_closed(); lv_obj_delete(panel); panel = NULL; }
static bool label_found;
static const char *wanted;
static void find_label(lv_obj_t *o) {
    if (lv_obj_check_type(o, &lv_label_class) && strstr(lv_label_get_text(o), wanted)) label_found = true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find_label(lv_obj_get_child(o, (int32_t)i));
}
static void look(void) { label_found = false; find_label(lv_screen_active()); }
static bool on_screen(const char *text) { wanted = text; on_ui(look); return label_found; }
static void capture(void *arg) { sim_display_snapshot(arg); }
static void picture(const char *name) { quiet(); vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name); delay_ms(20); }
static void debug_picture(void) { picture("debug-touch"); }
static void size(int level) { char a[64]; snprintf(a, sizeof a, "{\"id\":\"display.font_size\",\"value\":%d}", level); ok("settings.set", a); }
static void switch_to(const char *id) {
    char a[48]; snprintf(a, sizeof a, "{\"id\":\"%s\"}", id);
    CHECK(BY_HAND("system.switch_member", a) && g_rc.status == INK_RC_DONE, "switch %s", id);
    CHECK(WAIT_FOR(ink_member_space_open(), 5000), "member opens");
}
static void open_music(void) { ok("app.open", "{\"id\":\"music\"}"); CHECK(WAIT_FOR((on_ui(grab), strstr(snap, "\"page\":\"Music\"") != NULL), 8000), "music page"); quiet(); }
static double counter(const char *n) { return sim_metric_value(n); }
static void old_records(void) { /* what the old music (system/apps/music) wrote: path records keyed by crc32 + 1, last at id 1 */
    static const ink_db_def_t FAV = {.name = "music.favorites", .title = "收藏的歌曲", .scope = INK_DB_MEMBER, .schema = 1, .ai_read = true, .max_records = 100};
    static const ink_db_def_t LAST = {.name = "music.last", .title = "上次听的歌曲", .scope = INK_DB_MEMBER, .schema = 1, .max_records = 1};
    const char *fav = MINE "丙.wav", *last = MINE "乙.wav";
    ink_db_t *db = ink_db_open(&FAV, NULL);
    ink_db_write_t w = {.id = (ink_crc32(0, fav, strlen(fav)) & 0x7fffffff) + 1, .data = fav, .len = strlen(fav) + 1, .src = INK_SRC_SYSTEM};
    CHECK(db && ink_db_put(db, &w, NULL) == INK_DB_OK, "old favorite written"); ink_db_close(db);
    db = ink_db_open(&LAST, NULL);
    w = (ink_db_write_t){.id = 1, .data = last, .len = strlen(last) + 1, .src = INK_SRC_SYSTEM};
    CHECK(db && ink_db_put(db, &w, NULL) == INK_DB_OK, "old last song written"); ink_db_close(db);
}
static void main_phase(void) {
    /* 4. The old music's favorites and last song, written before the new music first runs. */
    old_records(); open_music();
    CHECK(shows("title", "乙"), "the old last song shows: %s", field("title"));
    ok("music.list", "{\"view\":\"list\",\"page\":4}"); quiet();
    ok("music.favorites", NULL); CHECK(strstr(g_result, MINE "丙.wav") && row_favorite(MINE "丙.wav"), "the old favorite is a favorite: %.150s", g_result);
    ok("music.favorite", "{\"path\":\"" MINE "丙.wav\"}");
    /* 8. Pictures at the default size; the m4a row says it cannot play, and tapping it says why. */
    ok("music.list", "{\"view\":\"list\",\"page\":3}"); quiet();
    CHECK(row_of(MINE "不能放.m4a") >= 0 && strstr(snap, "这种格式还不能放"), "m4a listed and marked");
    targets(); EpdRect r = T[ROW0 + row_of(MINE "不能放.m4a")]; touch(r.x + r.width / 2, r.y + r.height / 2);
    CHECK(shows("library", "这种格式还不能放"), "tapping it says why: %s", field("library"));
    CHECK(AI("music.play", "{\"path\":\"" MINE "不能放.m4a\"}") && g_rc.status == INK_RC_INVALID && strstr(g_rc.say, "还不能放"), "AI is told the same");
    /* Tags are read in the member's own folder as in the family's. */
    ok("music.list", "{\"view\":\"list\",\"page\":5}"); quiet();
    { char id[12]; int tr = row_of(MINE "带标签.mp3"); snprintf(id, sizeof id, "track%d", tr); CHECK(tr >= 0 && strstr(label(id), "合成音调 · 测试歌手"), "tags in /me/音乐: %s", label(id)); }
    ok("music.play", "{\"path\":\"/family/audio/tone-16000-1.flac\"}"); ok("music.pause", NULL); ok("music.list", "{\"view\":\"player\",\"page\":0}");
    picture("default-player"); targets();
    ok("music.list", "{\"view\":\"all\"}"); picture("default-all-folders"); targets();
    ok("music.list", "{\"path\":\"/family/audio/文件夹\"}"); picture("default-all-folder-open"); targets();
    ok("music.list", "{\"view\":\"player\"}");
    /* 1. The kiikoread walk by touch, then the same by AI: same results and snapshots. */
    walk(true); walk(false);
    for (int i = 0; i < 13; i++) CHECK(!strcmp(walk_log[0][i], walk_log[1][i]), "touch and AI agree at %d:\n%.120s\n%.120s", i, walk_log[0][i], walk_log[1][i]);
    /* 5. No rebinding: 200 progress refreshes add no callbacks, then one tap is one call. */
    ok("music.list", "{\"view\":\"player\"}"); ok("music.play", "{\"path\":\"" MINE "长歌.wav\"}"); ok("music.pause", NULL); quiet();
    on_ui(count_callbacks); int before = counted; double parts = counter("music.present.part");
    for (int i = 0; i < 200; i++) { char a[32]; snprintf(a, sizeof a, "{\"ms\":%d}", 1000 + i * 100); ok("music.seek", a); quiet(); }
    quiet(); on_ui(count_callbacks);
    CHECK(counter("music.present.part") - parts >= 200, "200 progress refreshes (%.0f)", counter("music.present.part") - parts);
    CHECK(counted == before, "no new event callbacks after 200 refreshes (%d -> %d)", before, counted);
    double calls = counter("music.touch_call"), resumed = counter("media.resumed");
    press(PLAY); CHECK(WAIT_FOR(status().state == INK_MEDIA_PLAYING, 2000), "one tap plays");
    CHECK(counter("music.touch_call") - calls == 1 && counter("media.resumed") - resumed == 1, "one tap, one call (%.0f, %.0f)", counter("music.touch_call") - calls, counter("media.resumed") - resumed);
    /* 6. While a song plays on the player, only the bar (and the lyric line) refresh; the mode only its control. */
    ok("music.play", "{\"path\":\"" MINE "长歌.wav\"}"); ok("music.seek", "{\"ms\":0}"); quiet(); targets();
    double page = counter("music.present.page"), whole = counter("music.present.whole"), part = counter("music.present.part");
    const EpdRect bar = T[BAR];
    for (int i = 0; i < 160 && status().state == INK_MEDIA_PLAYING && !strcmp(status().path, MINE "长歌.wav"); i++) {
        double seen = counter("music.present.part"); delay_ms(50);
        if (counter("music.present.part") > seen) {
            int y1 = (int)sim_metric_last("music.present_y1"), y2 = (int)sim_metric_last("music.present_y2");
            CHECK(y2 < bar.y + bar.height && y1 >= bar.y - 2 * ink_ui_px(INK_LINE_BODY), "only the bar band and the lyric line above it (%d-%d)", y1, y2);
        }
        if (status().position_ms > 7000) break;
    }
    CHECK(counter("music.present.part") - part >= 3, "the bar and lyric moved (%.0f)", counter("music.present.part") - part);
    CHECK(counter("music.present.page") == page && counter("music.present.whole") == whole, "no whole-page refresh while the song plays");
    /* The bar on the panel stays one piece: a frame that leaves the bar out (the play key, a lyric line) does not count as
     * having shown it, so the next sliver starts where the panel's bar ends. */
    for (int i = 0; i < 5; i++) { delay_ms(1300); ok("music.pause", NULL); quiet(); ok("music.play", NULL); quiet(); }
    delay_ms(2500); ok("music.pause", NULL);
    dark_in(bar.x, bar.x + bar.width, bar.y + bar.height / 3 + 1, bar.y + bar.height / 3 + 2);
    CHECK(bar_whole && band_dark > 0, "the bar on the panel has no gap (%d dark)", band_dark);
    ok("music.pause", NULL); quiet(); part = counter("music.present.part");
    ok("music.mode", "{\"mode\":\"cycle\"}"); quiet(); targets();
    CHECK(counter("music.present.part") == part + 1 && (int)sim_metric_last("music.present_y1") == T[MODE].y &&
          (int)sim_metric_last("music.present_y2") == T[MODE].y + T[MODE].height - 1, "mode refreshes only its control");
    ok("music.mode", "{\"mode\":\"list\"}"); quiet();
    ok("music.list", "{\"view\":\"list\",\"page\":4}"); quiet(); targets(); part = counter("music.present.part");
    { const int fr = row_of(MINE "乙.wav"); ok("music.favorite", "{\"path\":\"" MINE "乙.wav\"}"); quiet();
      CHECK(fr >= 0 && counter("music.present.part") == part + 1 && (int)sim_metric_last("music.present_y1") == T[ROW0 + fr].y &&
            (int)sim_metric_last("music.present_y2") == T[ROW0 + fr].y + T[ROW0 + fr].height - 1, "a favorite refreshes only its row");
      ok("music.favorite", "{\"path\":\"" MINE "乙.wav\"}"); quiet(); }
    /* How many refreshes each main action costs (recorded for the report). */
    {
        static const char *const steps[][3] = {
            {"play", "music.play", "{\"path\":\"" MINE "丙.wav\"}"}, {"pause", "music.pause", NULL}, {"seek", "music.seek", "{\"ms\":5000}"},
            {"resume", "music.play", NULL}, {"next", "music.next", NULL}, {"mode", "music.mode", "{\"mode\":\"one\"}"},
            {"favorite", "music.favorite", NULL}, {"unfavorite", "music.favorite", NULL}, {"page", "music.list", "{\"view\":\"list\",\"page\":0}"},
            {"all", "music.list", "{\"view\":\"all\"}"}, {"folder", "music.list", "{\"path\":\"/family/audio/文件夹\"}"},
            {"up", "music.list", "{\"path\":\"/\"}"}, {"player", "music.list", "{\"view\":\"player\"}"}, {"stop", "music.pause", NULL}};
        for (size_t i = 0; i < sizeof steps / sizeof *steps; i++) {
            double was_shown = counter("music.present.page") + counter("music.present.whole") + counter("music.present.part");
            ok(steps[i][1], steps[i][2]); settle();
            double n = counter("music.present.page") + counter("music.present.whole") + counter("music.present.part") - was_shown;
            char name[48]; snprintf(name, sizeof name, "music.refreshes.%s", steps[i][0]); ink_probe_value(name, n);
            CHECK(n == 1, "%s costs one refresh (%.0f)", steps[i][0], n);
        }
        ok("music.mode", "{\"mode\":\"list\"}"); quiet();
    }
    /* Table rows 12, 5 and 19: minus and plus step audio.volume through settings.set and redraw only the volume line;
     * the output capsule opens the Bluetooth page; the cover keys page the list on the player and in all songs. */
    ok("music.list", "{\"view\":\"list\",\"page\":0}"); quiet(); targets();
    { const int vol = (int)ink_setting_get("audio.volume"), want[2] = {vol < 6 ? 0 : vol - 6, vol < 6 ? 6 : vol};
      for (int i = 0; i < 2; i++) {
          double was = counter("music.present.part"); press(i ? PLUS : MINUS);
          CHECK(WAIT_FOR(ink_setting_get("audio.volume") == want[i], 2000), "volume key %d sets audio.volume to %d (%d)", i, want[i], (int)ink_setting_get("audio.volume"));
          quiet(); CHECK(counter("music.present.part") == was + 1 && (int)sim_metric_last("music.present_y1") == T[MINUS].y &&
                         (int)sim_metric_last("music.present_y2") == T[MINUS].y + T[MINUS].height - 1, "the volume key redraws only its line");
      }
      if (vol < 6) { char v[64]; snprintf(v, sizeof v, "{\"id\":\"audio.volume\",\"value\":%d}", vol); ok("settings.set", v); } }
    press(OUTPUT); on_ui(read_screen); CHECK(!strcmp(g_top, "SetBluetooth"), "the output capsule opens the Bluetooth page (%s)", g_top);
    open_music(); ok("music.list", "{\"view\":\"list\",\"page\":0}"); quiet();
    { double keyed = counter("music.touch_call"), shown = counter("music.present.whole") + counter("music.present.part") + counter("music.present.page");
      key(HAL_KEY_NEXT); quiet(); CHECK(shows("page", "第 2 页"), "the next key turns the page: %s", field("page"));
      key(HAL_KEY_PREV); quiet(); CHECK(shows("page", "第 1 页"), "the previous key turns back: %s", field("page"));
      CHECK(counter("music.touch_call") - keyed == 2 && counter("music.present.whole") + counter("music.present.part") + counter("music.present.page") - shown == 2,
            "each key is one call and one refresh"); }
    ok("music.list", "{\"view\":\"all\",\"page\":0}"); quiet();
    key(HAL_KEY_NEXT); quiet(); CHECK(shows("page", "第 2 页") && !strcmp(label("page"), "全部歌曲"), "the next key turns all songs too: %s", field("page"));
    ok("music.list", "{\"view\":\"player\"}"); quiet();
    /* 2. Playing on: leave the page, lock, open the reader; the song ends and the next follows the mode. */
    ok("music.play", "{\"path\":\"" MINE "乙.wav\"}"); ok("music.seek", "{\"ms\":17000}");
    test_start_home(); ink_power_release(INK_NEED_NO_LOCK); key(HAL_KEY_POWER); CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 3000), "locked");
    CHECK(WAIT_FOR(!strcmp(status().path, MINE "甲.wav"), 10000), "locked: the next song follows the list");
    key(HAL_KEY_POWER); CHECK(WAIT_FOR(strcmp(power_state(), "locked"), 5000), "awake"); ink_power_hold(INK_NEED_NO_LOCK);
    ok("app.open", "{\"id\":\"reader\"}"); ok("music.seek", "{\"ms\":19000}");
    CHECK(WAIT_FOR(!strcmp(status().path, MINE "长歌.wav"), 10000), "with the reader open the next song follows");
    open_music(); CHECK(shows("title", "长歌"), "back in music: the song now playing");
    ok("music.pause", NULL); quiet(); on_ui(pixels); uint32_t was = raster; on_ui(grab); char before_snap[2048]; snprintf(before_snap, sizeof before_snap, "%s", field("title"));
    ink_interrupt_t call = {.kind = INK_INT_CALL, .title = "SYNTH 来电", .body = "测试"};
    int id = ink_interrupt_raise(&call); CHECK(id > 0 && WAIT_FOR(ink_ui_overlay_up(), 3000), "call card over the page");
    ink_interrupt_end(id); CHECK(WAIT_FOR(!ink_ui_overlay_up(), 3000), "call card gone");
    on_ui(ai_open); delay_ms(300); on_ui(ai_close); quiet();
    on_ui(pixels); CHECK(raster == was && shows("title", before_snap), "the page is as it was after the card and the AI panel");
    /* 8. Pictures at the largest size. */
    size(3); open_music(); ok("music.list", "{\"view\":\"player\",\"page\":0}"); picture("largest-player"); targets();
    ok("music.list", "{\"view\":\"all\"}"); picture("largest-all-folders"); targets();
    ok("music.list", "{\"view\":\"player\"}"); size(1);
    /* 3. Members: the adult's place, the child's own songs, back to the adult's place, mode and favorites. */
    ok("music.mode", "{\"mode\":\"favorites\"}");
    ok("music.play", "{\"path\":\"" MINE "长歌.wav\"}"); ok("music.seek", "{\"ms\":60000}"); ok("music.pause", NULL);
    switch_to("kid"); open_music();
    bool own = false, adult = false;
    for (int at = 0; at < 4; at++) {
        char a[48]; snprintf(a, sizeof a, "{\"view\":\"all\",\"page\":%d}", at); ok("music.list", a);
        own = own || strstr(g_result, "孩子"); adult = adult || strstr(g_result, "长歌") || strstr(g_result, "甲");
    }
    CHECK(own && !adult, "the child sees only their own songs and the family's");
    CHECK(AI("music.play", "{\"path\":\"/members/owner/音乐/甲.wav\"}") && g_rc.status != INK_RC_DONE, "the child cannot play the adult's songs");
    ok("music.list", "{\"view\":\"player\",\"page\":0}"); picture("child-player"); targets();
    ok("music.play", "{\"path\":\"" MINE "孩子.wav\"}");
    switch_to("owner"); open_music(); ok("music.play", NULL);
    CHECK(WAIT_FOR(!strcmp(status().path, MINE "长歌.wav"), 3000) && status().position_ms >= 59000 && status().position_ms <= 61500, "the adult goes on at about a minute (%u)", (unsigned)status().position_ms);
    ok("music.now", NULL); CHECK(strstr(g_result, "\"mode\":\"favorites\""), "mode kept");
    ok("music.favorites", NULL); CHECK(strstr(g_result, "甲.wav"), "favorites kept");
    ok("music.seek", "{\"ms\":45000}"); ok("music.pause", NULL);
    CHECK(WAIT_FOR(status().state == INK_MEDIA_PAUSED, 2000), "paused at 45 s for the restart");
}
static const ink_db_def_t FAVORITES = {.name = "music.favorites", .title = "收藏的歌曲", .scope = INK_DB_MEMBER, .schema = 1, .ai_read = true, .max_records = MUSIC_MAX_TRACKS};
static int fill_favorites(bool fill) { /* records for songs that are not there, as many as the collection takes; or removes them */
    ink_db_t *db = ink_db_open(&FAVORITES, NULL);
    int n = 0;
    for (int i = 0; db && i < 300; i++) {
        char p[64]; snprintf(p, sizeof p, MINE "满/%03d.wav", i);
        ink_db_write_t w = {.id = (ink_crc32(0, p, strlen(p)) & 0x7fffffff) + 1, .data = p, .len = strlen(p) + 1, .src = INK_SRC_SYSTEM};
        int r = fill ? ink_db_put(db, &w, NULL) : ink_db_del(db, w.id, INK_SRC_SYSTEM);
        if (r == INK_DB_OK) n++;
        else if (fill) break;
    }
    ink_db_close(db);
    return n;
}
static int favorite_records(void) {
    ink_db_t *db = ink_db_open(&FAVORITES, NULL);
    ink_db_cursor_t cur = {0}; ink_rec_t rec; int n = 0;
    while (db && ink_db_next(db, &cur, &rec, NULL, 0) == 1) n++;
    ink_db_close(db);
    return n;
}
static void restart_phase(void) {
    open_music(); CHECK(shows("title", "长歌"), "after a restart the last song shows: %s", field("title"));
    ok("music.now", NULL); CHECK(strstr(g_result, "\"mode\":\"favorites\""), "the mode survives a restart");
    ok("music.play", NULL);
    CHECK(WAIT_FOR(status().state == INK_MEDIA_PLAYING, 3000) && !strcmp(status().path, MINE "长歌.wav") && status().position_ms >= 44000 && status().position_ms <= 46500,
          "play goes on where it was (%s %u)", status().path, (unsigned)status().position_ms);
    ok("music.pause", NULL);
    /* Favorites hold as many songs as the library (200). Full, a favorite more is refused and said on the page and to the
     * AI; the row, the loop and the stored favorites stay in step, and nothing is lost silently. */
    const int had = favorite_records(), added = fill_favorites(true);
    CHECK(had + added == MUSIC_MAX_TRACKS, "the favorites take %d songs (%d + %d)", MUSIC_MAX_TRACKS, had, added);
    ok("music.list", "{\"view\":\"list\",\"page\":4}"); quiet();
    CHECK(AI("music.favorite", "{\"path\":\"" MINE "乙.wav\"}") && g_rc.status == INK_RC_FAILED && strstr(g_rc.say, "收藏满了"), "full: the AI is told (%s %s)", ink_rc_name(g_rc.status), g_rc.say);
    quiet(); CHECK(!row_favorite(MINE "乙.wav") && shows("library", "收藏满了"), "full: the row stays and the page says why: %s", field("library"));
    row_tap(true, MINE "乙.wav", 4, true);
    CHECK(!row_favorite(MINE "乙.wav") && favorite_records() == MUSIC_MAX_TRACKS, "full: a tap on the row changes nothing either");
    CHECK(AI("music.favorite", "{\"path\":\"" MINE "乙.wav\"}") && g_rc.status == INK_RC_FAILED, "full: asked again, still refused (not taken for a removal)");
    CHECK(fill_favorites(false) == added, "the extra records removed");
    ok("music.favorite", "{\"path\":\"" MINE "乙.wav\"}"); quiet();
    CHECK(strstr(g_result, "\"favorite\":true") && row_favorite(MINE "乙.wav") && !shows("library", "收藏满了"), "with room again it is a favorite and the notice goes");
    ok("music.favorite", "{\"path\":\"" MINE "乙.wav\"}"); quiet();
    /* Entering does not scan the library again unless its folders changed: a song recorded into the member's music
     * folder (file.changed) is there at the next entry, read once. */
    test_start_home(); open_music(); test_start_home(); open_music();
    const double scans = counter("music.rescan");
    test_start_home(); open_music();
    CHECK(counter("music.rescan") == scans, "an unchanged library is not scanned again on entry (%.0f)", counter("music.rescan") - scans);
    test_start_home();
    CHECK(BY_HAND("audio.record", "{\"path\":\"" MINE "录音.opus\"}") && g_rc.status == INK_RC_DONE, "recording into the music folder (%s)", g_rc.detail);
    delay_ms(8000); CHECK(BY_HAND("audio.record_stop", NULL) && g_rc.status == INK_RC_DONE, "recording stops");
    { ink_fs_stat_t fs; CHECK(WAIT_FOR(!ink_fs_stat(MINE "录音.opus", &fs) && fs.size > 1000, 3000), "the recording is on the card"); }
    delay_ms(500); open_music();
    CHECK(counter("music.rescan") == scans + 1, "the changed folder is scanned once (%.0f)", counter("music.rescan") - scans);
    /* Table row 25: the card goes while a song plays (an Opus file says so; see the report): the lyric line says the
     * song cannot be read, on the panel and in the snapshot. */
    ok("music.list", "{\"view\":\"player\",\"page\":0}"); ok("music.play", "{\"path\":\"" MINE "录音.opus\"}"); targets();
    const int lyric_top = T[BAR].y - ink_ui_px(INK_LINE_BODY) + 2, lyric_bottom = T[BAR].y - 2;
    CHECK(dark_in(T[BAR].x, T[BAR].x + T[BAR].width, lyric_top, lyric_bottom) == 0, "no lyric for this song");
    delay_ms(1500); sim_sd_set_present(false);
    CHECK(WAIT_FOR(status().state != INK_MEDIA_PLAYING, 8000), "the card goes: the song stops");
    quiet(); CHECK(shows("library", "读不了这首歌"), "the snapshot says it cannot be read: %s", field("library"));
    CHECK(dark_in(T[BAR].x, T[BAR].x + T[BAR].width, lyric_top, lyric_bottom) > 0, "the panel says it on the lyric line");
    sim_sd_set_present(true); delay_ms(1500);
}
static void test(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "boot opens member"); test_start_home();
    ink_power_hold(INK_NEED_AWAKE); ink_power_hold(INK_NEED_NO_LOCK); /* no automatic lock in the middle of a walk */
    top = ink_ui_px(INK_STATUS_BAR);
    if (!strcmp(phase, "main")) main_phase();
    else if (!strcmp(phase, "restart")) restart_phase();
    else if (!strcmp(phase, "nofont")) {
        /* 7. No font anywhere: the card files' notice says what is missing and that it comes by itself (D-138), as
         * the reader's does; the AI can still play. */
        open_music(); CHECK(WAIT_FOR(shows("library", "音乐要用的字体还没放到存储卡上") && strstr(snap, "card.files=missing"), 10000),
                            "snapshot names the missing font and the card files' state: %s", field("library"));
        CHECK(on_screen("字体还没放到存储卡上"), "a page of explanation, not a blank screen");
        picture("default-no-font"); ok("music.play", "{\"path\":\"" FIRST "\"}"); CHECK(WAIT_FOR(status().state == INK_MEDIA_PLAYING, 3000), "music.play still plays");
        ink_media_stop(); size(3); open_music(); picture("largest-no-font"); size(1);
    } else if (!strcmp(phase, "empty")) {
        open_music(); CHECK(shows("library", "还没有歌") && on_screen("还没有歌") && on_screen("用传书传上来"), "empty library: where songs go");
        CHECK(!strstr(snap, "/me/音乐"), "no path string for people");
        picture("default-empty"); size(3); open_music(); picture("largest-empty"); size(1);
    } else if (!strcmp(phase, "flat")) {
        open_music(); ok("music.list", "{\"view\":\"all\"}"); quiet();
        CHECK(shows("page", "第 1 页") && !strstr(snap, "\"directory\"") && row_of(MINE "甲.wav") >= 0, "flat list without folders");
        picture("default-all-flat"); targets(); size(3); open_music(); ok("music.list", "{\"view\":\"all\"}"); picture("largest-all-flat"); targets(); size(1);
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(read_env, NULL);
    product_main(); osal_task_cfg_t c = {.name = "musicplayer", .fn = test, .stack_bytes = 32768, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&c, NULL) != OSAL_OK) osal_panic("music player test task");
}
