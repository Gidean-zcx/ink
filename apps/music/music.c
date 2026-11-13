/* ui-scale-runtime: dimension text_percent The 字号 percentage (90-120, a child 110) in this panel's pixels, as Mogo does. */
/* 音乐: the kiikoread player's page and library (third_party/kiikoread_music) on the graft surface, playing through
 * the media service. Touch and AI share the music.* capabilities. The library, the page, every capability and the
 * end-of-song follow-up run on the BG executor; the UI task only presents finished pixels. */
#ifdef ESP_PLATFORM
#include <esp_attr.h> /* INK_PSRAM_BSS=EXT_RAM_BSS_ATTR comes from music.cmake */
#endif
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <graft.h>
#include "ink/probe.h"
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/crc.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/media.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/ui_semantics.h"
#include <ink/ui_theme.h>
#include <port/music_page.h>
#include <music_lyrics.h>
void ink_music_tags(const char *path, char *title, char *artist, char *album, size_t cap);
/* Mogo's TrueType engine and its member-scoped font reads (apps/mogo/port/runtime.c), shared, never copied. */
int ttf_font_open(const char *path);
void ink_mogo_font_shutdown(void);
const char *app_settings_font_path(void);
void ink_mogo_owner(const ink_member_identity_t *owner);
void ink_mogo_dimensions(int w, int h, int px);

enum { ROWS_MAX = 12 };
typedef struct {
    char owner[INK_MEMBER_ID_MAX], title[96], artist[96], album[96], progress[32], lyric[192], dir[160], notice[96];
    const char *state, *mode, *output;
    bool font, empty, all, favorite, limited;
    int page, pages, count, rows;
    struct { char name[96], detail[96], path[128]; bool folder; } row[ROWS_MAX];
} music_view_t;
static const ink_db_def_t FAV = {.name = "music.favorites", .title = "收藏的歌曲", .scope = INK_DB_MEMBER, .schema = 1, .ai_read = true, .max_records = MUSIC_MAX_TRACKS};
static const ink_db_def_t LAST = {.name = "music.last", .title = "上次听的歌曲", .scope = INK_DB_MEMBER, .schema = 1, .max_records = 1};
static const ink_db_def_t MODE = {.name = "music.mode", .title = "循环模式", .scope = INK_DB_MEMBER, .schema = 1, .max_records = 1};
static const char *const MODES[] = {"list", "one", "shuffle", "favorites"};
static const ink_activity_t PAGE;

/* What plays after what music started (BG, like the media.finished subscription that reads it). */
static struct { char member[INK_MEMBER_ID_MAX], playing[INK_FS_PATH_MAX], next[INK_FS_PATH_MAX], why[24]; } S INK_PSRAM_BSS;
/* The BG executor's. */
static struct {
    char member[INK_MEMBER_ID_MAX], track[INK_FS_PATH_MAX], album[96], single_title[INK_FS_PATH_MAX], single_artist[96];
    ink_media_status_t st;
    ink_member_identity_t owner;
    uint8_t *pixels;
    int w, h, want, volume, route, percent, screen_w, tap_x, tap_y, key;
    bool open, font, pending, switching, stale; /* switching: music asked for another song; stale: its folders changed */
    ink_source_t src;
    lv_area_t area, shown;               /* what the next frame changes; what the frame on its way changed */
    ink_graft_refresh_t refresh, shown_refresh;
    ink_exec_job_t open_job, close_job, tick_job, tap_job, key_job, present_job;
    music_view_t view;
} B INK_PSRAM_BSS;
static volatile bool presenting; /* BG sets it, the UI task clears it */
/* The UI task's. */
static struct { lv_obj_t *screen, *notice[2]; lv_timer_t *timer; bool active; bool drawn_font, drawn_empty; } U;
static music_view_t V INK_PSRAM_BSS;
static char result[6144] INK_PSRAM_BSS;

/* ---- what the page and library ask of this system ---- */
static int text_percent(void) {
    static const int percent[] = {90, 100, 110, 120}; /* system/ui/theme.c TEXT_PERCENT; a child reads at 110 */
    int level = ink_member_current()->role == INK_ROLE_CHILD ? 2 : (int)ink_setting_get("display.font_size");
    return percent[level < 0 ? 0 : level > 3 ? 3 : level];
}
int ink_music_percent(void) { return B.percent ? B.percent : text_percent(); }
int ink_music_px(int px) { return B.screen_w ? (px * B.screen_w + INK_CANVAS_W / 2) / INK_CANVAS_W : px; }
uint8_t ink_music_gray(int which) {
    return which == 0 ? 255 : which == 1 ? 0 : lv_color_luminance(which == 2 ? ink_ui_secondary() : ink_ui_separator());
}
uint32_t ink_music_random(void) { /* the app framework has no random source: xorshift stirred with the clock */
    static uint32_t x = 2463534242u;
    x ^= (uint32_t)ink_now_us(); x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x;
}
const char *music_root(void) { return MUSIC_ROOT; }
const char *music_root_of(const char *path) { return strncmp(path, MUSIC_FAMILY_ROOT "/", sizeof MUSIC_FAMILY_ROOT) ? MUSIC_ROOT : MUSIC_FAMILY_ROOT; }
bool music_playable(const char *path) {
    const char *e = strrchr(path, '.');
    return e && strcasecmp(e, ".m4a") && strcasecmp(e, ".aac"); /* no AAC decoder in this firmware yet */
}
music_state_t music_state(void) {
    return B.st.state == INK_MEDIA_PLAYING ? MUSIC_PLAYING : B.st.state == INK_MEDIA_PAUSED ? MUSIC_PAUSED : MUSIC_IDLE;
}
const char *music_error(void) { return ""; }
uint32_t music_elapsed_ms(void) { return B.st.position_ms; }
uint32_t music_duration_ms(void) { return B.st.duration_ms ? B.st.duration_ms : music_track(music_track_index()) ? music_track(music_track_index())->duration_ms : 0; }
uint8_t music_volume(void) { return (uint8_t)ink_setting_get("audio.volume"); }
music_output_t music_output(void) { char r[16] = ""; ink_state_get_str(ink_state_find("audio.route"), r, sizeof r); return strcmp(r, "speaker") ? MUSIC_OUT_BT : MUSIC_OUT_SPK; }
void music_play_index(int index) { B.want = index; }
static uint32_t favorite_id(const char *p) { return (ink_crc32(0, p, strlen(p)) & 0x7fffffff) + 1; }
static bool favorite_of(ink_db_t *db, const char *path) {
    char got[INK_FS_PATH_MAX];
    ink_rec_t rec;
    if (!db || ink_db_get(db, favorite_id(path), &rec, got, sizeof got - 1) != INK_DB_OK) return false;
    got[rec.len < sizeof got - 1 ? rec.len : sizeof got - 1] = 0;
    return !strcmp(got, path);
}
void music_port_favorites_load(const music_track_t *tracks, int count, uint8_t *fav) {
    ink_db_t *db = ink_db_open(&FAV, NULL);
    for (int i = 0; i < count; i++) fav[i] = favorite_of(db, tracks[i].path);
    ink_db_close(db);
}
bool music_port_favorite_store(const char *path, bool favorite) { /* false when the collection refuses (full) */
    ink_db_t *db = ink_db_open(&FAV, NULL);
    ink_db_write_t w = {.id = favorite_id(path), .data = path, .len = strlen(path) + 1, .src = B.src};
    const int r = !db ? INK_DB_ERR_ARG : favorite ? ink_db_put(db, &w, NULL) : ink_db_del(db, w.id, B.src);
    ink_db_close(db);
    return r == INK_DB_OK || r == INK_DB_ERR_NOT_FOUND;
}
static void record(const ink_db_def_t *def, const char *text) {
    ink_db_t *db = ink_db_open(def, NULL);
    ink_db_write_t w = {.id = 1, .data = text, .len = strlen(text) + 1, .src = INK_SRC_SYSTEM};
    if (db) ink_db_put(db, &w, NULL);
    ink_db_close(db);
}
static void recall(const ink_db_def_t *def, char *out, size_t cap) {
    ink_db_t *db = ink_db_open(def, NULL);
    ink_rec_t rec;
    out[0] = 0;
    if (db && ink_db_get(db, 1, &rec, out, cap - 1) == INK_DB_OK) out[rec.len < cap - 1 ? rec.len : cap - 1] = 0;
    ink_db_close(db);
}
void music_port_mode_store(music_mode_t mode) { record(&MODE, MODES[mode]); }

/* ---- the BG side ---- */
static void draw(app_redraw_t kind);
static bool ensure(void) { /* this member's library: scanned on first use, again only when its folders change */
    const char *id = ink_member_current()->id;
    if (!strcmp(B.member, id)) return false;
    char last[INK_FS_PATH_MAX], mode[16];
    music_library_reset(); music_page_reset(); music_lyrics_clear(); B.stale = false; /* entering scans the empty library */
    B.track[0] = B.album[0] = 0;
    recall(&LAST, last, sizeof last); recall(&MODE, mode, sizeof mode);
    int m = 0;
    while (m < 4 && strcmp(mode, MODES[m])) m++;
    music_library_restore(last, m);
    snprintf(B.member, sizeof B.member, "%s", id);
    if (!B.w) B.w = INK_CANVAS_W, B.h = INK_CANVAS_H - INK_STATUS_BAR; /* before the page first opens: the surface it will get */
    music_page_enter(B.w, B.h);
    return true;
}
static void refresh(void) { /* what the media service plays now, as the library and the page see it */
    ink_media_status(&B.st);
    if (B.st.path[0] && strcmp(B.track, B.st.path)) {
        char title[96], artist[96];
        snprintf(B.track, sizeof B.track, "%s", B.st.path);
        ink_music_tags(B.track, title, artist, B.album, sizeof title);
        snprintf(B.single_title, sizeof B.single_title, "%s", title[0] ? title : strrchr(B.track, '/') ? strrchr(B.track, '/') + 1 : B.track);
        snprintf(B.single_artist, sizeof B.single_artist, "%s", artist);
        music_lyrics_load(B.track, B.st.duration_ms);
    }
    music_library_now(music_state(), B.st.path, B.single_title, B.single_artist);
}
static void pick(void) { /* music_library_pick runs the original finish_track rule ahead of the song's end */
    const int now = music_track_index();
    int next = music_library_pick(), skipped = 0;
    /* A format we cannot play is passed over; shuffle never lands back on the song that is playing. */
    for (; next >= 0 && skipped < music_track_count() && (next == now ? skipped && music_mode() == MUSIC_MODE_SHUFFLE : !music_playable(music_track(next)->path)); skipped++) {
        music_library_now(MUSIC_PLAYING, music_track(next)->path, "", "");
        next = music_library_pick();
    }
    if (skipped && now >= 0) music_library_now(music_state(), music_track(now)->path, "", "");
    const music_track_t *t = next >= 0 ? music_track(next) : NULL;
    snprintf(S.next, sizeof S.next, "%.127s", t ? t->path : ""); /* library paths come from the file index: under 128 */
}
static void notice(const char *text) { /* a notice on the lyric line, or none: only that line is redrawn */
    if (!text && !music_page_notice()[0]) return;
    music_page_message(text ? TR(text) : NULL);
    draw(music_page_changed("message", NULL));
}
static void rescan(void) { if (B.stale) B.stale = false, music_scan(), ink_probe_count("music.rescan"); } /* only new or changed files are read */
static void started(const char *path) { /* music put this song on: it follows the mode at its end */
    snprintf(S.member, sizeof S.member, "%s", ink_member_current()->id); snprintf(S.playing, sizeof S.playing, "%s", path);
    if (music_find_path(path) >= 0) record(&LAST, path);
    notice(NULL);
    pick();
}
static void fill_view(void) {
    music_view_t *v = &B.view;
    const int index = music_track_index();
    music_row_t rows[ROWS_MAX];
    int n = music_page_rows(rows, ROWS_MAX, &v->page, &v->pages);
    snprintf(v->owner, sizeof v->owner, "%s", B.member); snprintf(v->title, sizeof v->title, "%s", music_title());
    snprintf(v->artist, sizeof v->artist, "%s", music_artist()); snprintf(v->album, sizeof v->album, "%s", B.st.path[0] ? B.album : "");
    snprintf(v->lyric, sizeof v->lyric, "%s", music_lyrics_at(B.st.position_ms)); snprintf(v->dir, sizeof v->dir, "%s", music_page_dir());
    snprintf(v->progress, sizeof v->progress, "%u:%02u / %u:%02u", (unsigned)(B.st.position_ms / 60000), (unsigned)(B.st.position_ms / 1000 % 60),
             (unsigned)(music_duration_ms() / 60000), (unsigned)(music_duration_ms() / 1000 % 60));
    v->state = music_state() == MUSIC_PLAYING ? TR_NOOP("正在放") : music_state() == MUSIC_PAUSED ? TR_NOOP("已暂停") : TR_NOOP("没在放");
    v->mode = music_mode_label();
    v->output = music_output() == MUSIC_OUT_SPK ? TR_NOOP("扬声器") : TR_NOOP("耳机");
    v->favorite = index >= 0 && music_favorite(index);
    v->count = music_track_count(); v->limited = music_library_truncated(); v->font = B.font; v->all = music_page_all();
    v->empty = !v->count && music_state() == MUSIC_IDLE;
    snprintf(v->notice, sizeof v->notice, "%s", music_page_notice());
    v->rows = n;
    for (int i = 0; i < n; i++) {
        const music_track_t *t = music_track(rows[i].index);
        v->row[i].folder = !t;
        snprintf(v->row[i].name, sizeof v->row[i].name, "%s", t ? t->title : rows[i].folder);
        snprintf(v->row[i].path, sizeof v->row[i].path, "%.127s", t ? t->path : rows[i].folder);
        if (!t) snprintf(v->row[i].detail, sizeof v->row[i].detail, TR("%d 首"), rows[i].count);
        else {
            char d[192];
            snprintf(d, sizeof d, "%s%.64s%s%s%s", t->artist[0] ? " · " : "", t->artist, music_favorite(rows[i].index) ? TR(" · 已收藏") : "",
                     rows[i].index == index ? TR(" · 正在放") : "", music_playable(t->path) ? "" : TR(" · 这种格式还不能放"));
            snprintf(v->row[i].detail, sizeof v->row[i].detail, "%.90s", strncmp(d, " · ", 4) ? d : d + 4);
        }
    }
}
static void presented_ui(void *arg);
static void flush(void) { /* drawn once the UI task has copied the last frame */
    if (!B.pending || !B.open || __atomic_load_n(&presenting, __ATOMIC_ACQUIRE)) return;
    if (B.font) music_page_draw(B.pixels), music_page_presented((EpdRect){B.area.x1, B.area.y1, B.area.x2 - B.area.x1 + 1, B.area.y2 - B.area.y1 + 1});
    fill_view();
    B.shown = B.area; B.shown_refresh = B.refresh; B.pending = false;
    __atomic_store_n(&presenting, true, __ATOMIC_RELEASE);
    if (ink_exec_post(INK_EXEC_UI, &B.present_job, presented_ui, NULL)) __atomic_store_n(&presenting, false, __ATOMIC_RELEASE);
}
static void draw(app_redraw_t kind) { /* render the frame and hand only what changed to the UI task */
    if (!B.open || kind == APP_REDRAW_NONE) return;
    EpdRect r = kind == APP_REDRAW_PAGE ? (EpdRect){0, 0, B.w, B.h} : music_page_draw(NULL);
    lv_area_t a = {r.x, r.y, r.x + r.width - 1, r.y + r.height - 1};
    ink_graft_refresh_t refresh = kind == APP_REDRAW_PAGE ? INK_GRAFT_PAGE : r.width * r.height >= B.w * B.h / 2 ? INK_GRAFT_WHOLE : INK_GRAFT_PART;
    if (B.pending) {
        lv_area_t u = B.area;
        a = (lv_area_t){u.x1 < a.x1 ? u.x1 : a.x1, u.y1 < a.y1 ? u.y1 : a.y1, u.x2 > a.x2 ? u.x2 : a.x2, u.y2 > a.y2 ? u.y2 : a.y2};
        if (B.refresh < refresh) refresh = B.refresh;
    }
    B.area = a; B.refresh = refresh; B.pending = true;
    flush();
}
static void after_present(void *arg) { (void)arg; flush(); }
static ink_exec_job_t after_job;
static void opened_bg(void *arg) {
    (void)arg;
    const bool fresh = ensure();
    B.pixels = ink_app_alloc((size_t)B.w * B.h, true);
    B.open = B.pixels != NULL;
    if (!B.open) return;
    ink_music_canvas(B.w, B.h); ink_mogo_owner(&B.owner); ink_mogo_dimensions(B.w, B.h, B.percent);
    B.font = ttf_font_open(app_settings_font_path()) == 0;
    B.volume = music_volume(); B.route = music_output();
    refresh();
    if (!fresh) rescan(), music_page_enter(B.w, B.h);
    pick();
    draw(APP_REDRAW_PAGE);
}
static void closed_bg(void *arg) {
    (void)arg;
    B.open = B.pending = false;
    ink_mogo_font_shutdown(); ink_app_free(B.pixels);
    B.pixels = NULL; memset(&B.view, 0, sizeof B.view);
}
static void tick_bg(void *arg) {
    (void)arg;
    if (!B.open || B.switching) return; /* the stop between two songs is not drawn: the new song is, once */
    ensure();
    refresh();
    if (B.volume != music_volume() || B.route != (int)music_output()) {
        const bool volume = B.volume != music_volume();
        B.volume = music_volume(); B.route = music_output();
        draw(music_page_changed(volume ? "volume" : "output", NULL));
    }
    draw(music_page_tick());
}
static void tap_bg(void *arg) { (void)arg; B.src = INK_SRC_TOUCH; if (B.open && B.font) music_page_tap(B.tap_x, B.tap_y); }
static void key_bg(void *arg) { (void)arg; B.src = INK_SRC_KEY; if (B.open && B.font) music_page_key(B.key); }
static void advanced_bg(void *arg) { /* the next song started, or the song ended: what the page and library follow */
    (void)arg;
    char playing[INK_FS_PATH_MAX], why[24];
    snprintf(playing, sizeof playing, "%s", S.playing); snprintf(why, sizeof why, "%s", S.why);
    if (strcmp(B.member, S.member)) return;
    if (!strcmp(why, "read_failed")) music_page_message(""), draw(music_page_changed("message", NULL));
    if (playing[0] && !strcmp(why, "finished")) {
        music_library_now(MUSIC_PLAYING, playing, B.single_title, B.single_artist);
        if (music_find_path(playing) >= 0) record(&LAST, playing);
        pick();
    }
    draw(music_page_changed("play", NULL));
}
static void finished(void *ctx, const char *id, const ink_args_t *a) { /* BG: the next song was picked when this one began */
    (void)ctx; (void)id;
    char why[24] = "", path[INK_FS_PATH_MAX] = "", member[INK_MEMBER_ID_MAX] = "", next[INK_FS_PATH_MAX] = "";
    ink_args_get_str(a, "why", why, sizeof why); ink_args_get_str(a, "path", path, sizeof path); ink_args_get_str(a, "member", member, sizeof member);
    const bool ours = !strcmp(S.member, member) && !strcmp(S.playing, path);
    if (ours) snprintf(S.why, sizeof S.why, "%s", why);
    if (ours && !strcmp(why, "finished")) {
        snprintf(next, sizeof next, "%s", S.next); snprintf(S.playing, sizeof S.playing, "%s", S.next);
        S.next[0] = 0;
    }
    if (next[0]) ink_media_play(next, 0);
    if (ours) advanced_bg(NULL);
}
static void state_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (B.open) tick_bg(NULL);
}
static void files_changed(void *ctx, const char *id, const ink_args_t *a) { /* BG: a path over or under a music folder */
    char path[INK_FS_PATH_MAX] = "";
    (void)ctx; (void)id; ink_args_get_str(a, "path", path, sizeof path);
    const size_t n = strlen(path), me = sizeof MUSIC_ROOT - 1, family = sizeof MUSIC_FAMILY_ROOT - 1;
    B.stale |= !strncmp(path, MUSIC_ROOT, n < me ? n : me) || !strncmp(path, MUSIC_FAMILY_ROOT, n < family ? n : family);
}

/* ---- the capabilities (BG) ---- */
static void request_done(void *ctx, const ink_receipt_t *rc) {
    (void)ctx; /* a volume change made by a tap redraws its line */
    if (rc->status == INK_RC_DONE && B.volume != music_volume()) tick_bg(NULL);
}
bool music_request(const char *cap, const char *fmt, ...) {
    char args[256] = "{}";
    va_list ap;
    va_start(ap, fmt);
    if (fmt) vsnprintf(args, sizeof args, fmt, ap);
    va_end(ap);
    ink_call_opts_t o = {.src = B.src, .args_json = args, .reply_exec = INK_EXEC_BG, .done = request_done};
    ink_probe_count("music.touch_call");
    return !ink_cap_call(cap, NULL, &o);
}
static void jfield(ink_jw_t *w, const char *key, const char *text) { ink_jw_raw(w, key); ink_jw_str(w, text); }
bool music_request_path(const char *cap, const char *path) {
    char args[INK_FS_PATH_MAX + 32];
    ink_jw_t w;
    ink_jw_init(&w, args, sizeof args); jfield(&w, "{\"path\":", path); ink_jw_raw(&w, "}");
    return !w.overflow && music_request(cap, "%s", args);
}
static void say(ink_call_t *c, ink_rc_t status, const char *text) {
    c->rc.status = status;
    snprintf(c->rc.say, sizeof c->rc.say, "%s", TR(text));
}
static void forwarded(void *ctx, const ink_receipt_t *rc) {
    ink_call_t *c = ctx;
    const char *id = c->cap->id;
    c->rc.status = rc->status; B.switching = false;
    snprintf(c->rc.say, sizeof c->rc.say, "%s", rc->say); snprintf(c->rc.detail, sizeof c->rc.detail, "%s", rc->detail);
    refresh();
    if (rc->status == INK_RC_DONE && (!strcmp(id, "music.play") || !strcmp(id, "music.next") || !strcmp(id, "music.prev")) && B.st.path[0]) started(B.st.path);
    else if (rc->status != INK_RC_DONE && strcmp(id, "music.seek")) {
        music_page_message(TR("这首歌放不了"));
        snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("这首歌放不了"));
    }
    ink_cap_done(c);
    draw(music_page_changed(!strcmp(id, "music.seek") ? "seek" : "state", NULL)); /* the same song: its key; another: the page */
}
static void forward(ink_call_t *c, const char *target, const ink_args_t *a) {
    ink_call_opts_t o = {.src = c->src, .done = forwarded, .ctx = c, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_BG};
    ink_cap_defer(c);
    if (ink_cap_call(target, a, &o) < 0) {
        c->rc.status = INK_RC_FAILED; B.switching = false;
        ink_cap_done(c);
    }
}
static void play(ink_call_t *c, const char *path, int64_t start) {
    ink_args_t a;
    ink_args_init(&a);
    if (!path[0]) return say(c, INK_RC_INVALID, TR_NOOP("曲库里还没有歌"));
    if (!music_playable(path)) return notice(TR_NOOP("这种格式还不能放")), say(c, INK_RC_INVALID, TR_NOOP("这种格式还不能放"));
    ink_args_str(&a, "path", path);
    if (start >= 0) ink_args_int(&a, "start_ms", start);
    snprintf(S.playing, sizeof S.playing, "%s", path); S.next[0] = 0; B.switching = true; /* a song ending now must not start its successor over this */
    forward(c, "media.play", &a);
}
static const char *under(const char *path) { /* a library folder, relative to its root ("/" for the root), or NULL */
    const char *roots[] = {MUSIC_ROOT, MUSIC_FAMILY_ROOT};
    for (int i = 0; i < 2; i++) {
        size_t n = strlen(roots[i]);
        if (!strncmp(path, roots[i], n) && (!path[n] || path[n] == '/')) return path[n] && path[n + 1] ? path + n + 1 : "/";
    }
    return path[0] == '/' && path[1] ? NULL : path;
}
static void list_result(ink_call_t *c) {
    ink_jw_t w;
    ink_jw_init(&w, result, sizeof result);
    jfield(&w, "{\"path\":", B.view.all ? (B.view.dir[0] ? B.view.dir : "/") : "player"); ink_jw_raw(&w, ",\"items\":[");
    for (int i = 0; i < B.view.rows; i++) {
        jfield(&w, i ? ",{\"path\":" : "{\"path\":", B.view.row[i].path); jfield(&w, ",\"title\":", B.view.row[i].name); jfield(&w, ",\"detail\":", B.view.row[i].detail);
        ink_jw_raw(&w, B.view.row[i].folder ? ",\"directory\":true}" : ",\"directory\":false}");
    }
    ink_jw_raw(&w, "],\"page\":"); ink_jw_int(&w, B.view.page); ink_jw_raw(&w, ",\"pages\":"); ink_jw_int(&w, B.view.pages);
    ink_jw_raw(&w, ",\"count\":"); ink_jw_int(&w, B.view.count);
    ink_jw_raw(&w, B.view.page + 1 < B.view.pages ? ",\"more\":true" : ",\"more\":false");
    ink_jw_raw(&w, B.view.limited ? ",\"limited\":true}" : ",\"limited\":false}");
    if (!w.overflow) ink_cap_result(c, result);
}
static void run(ink_call_t *c) {
    const char *id = c->cap->id + 6;
    char path[INK_FS_PATH_MAX] = "", arg[16] = "";
    int64_t page = -1;
    B.src = c->src;
    ensure();
    refresh();
    ink_args_get_str(&c->args, "path", path, sizeof path);
    if (!strcmp(id, "list")) {
        ink_args_get_str(&c->args, "view", arg, sizeof arg);
        ink_args_get_int(&c->args, "page", &page);
        const char *dir = path[0] ? under(path) : NULL;
        if (path[0] && !dir) return say(c, INK_RC_INVALID, TR_NOOP("只能看 我的音乐 和 全家的音乐"));
        if (!B.open) rescan();
        int view = !strcmp(arg, "player") ? 0 : !strcmp(arg, "list") ? 1 : !strcmp(arg, "all") || dir ? 2 : -1;
        app_redraw_t kind = music_page_list(view, page > 10000 ? 10000 : (int)page, dir);
        draw(kind);
        fill_view();
        list_result(c);
    } else if (!strcmp(id, "play")) {
        ink_fs_stat_t st;
        char last[INK_FS_PATH_MAX];
        const char *dir = path[0] ? under(path) : NULL;
        const bool asked = path[0];
        if (!path[0] && B.st.state == INK_MEDIA_PAUSED) return forward(c, "media.resume", NULL);
        if (!path[0] && B.st.state == INK_MEDIA_PLAYING) return;
        recall(&LAST, last, sizeof last);
        if (!path[0]) snprintf(path, sizeof path, "%s", B.st.path[0] ? B.st.path : last);
        if ((dir && !strcmp(dir, "/")) || !path[0] || (!ink_fs_stat(path, &st) && st.is_dir)) {
            const size_t n = strlen(path);
            const music_track_t *t = NULL;
            for (int i = 0; i < music_track_count() && !t; i++)
                if (!n || (!strncmp(music_track(i)->path, path, n) && music_track(i)->path[n] == '/')) t = music_track(i);
            snprintf(path, sizeof path, "%.127s", t ? t->path : "");
        }
        /* open_track's rule: the last song goes on where it was, another starts from the beginning */
        play(c, path, !asked || !strcmp(path, last) || !strcmp(path, B.st.path) ? -1 : 0);
    } else if (!strcmp(id, "pause")) forward(c, "media.pause", NULL);
    else if (!strcmp(id, "seek")) forward(c, "media.seek", &c->args);
    else if (!strcmp(id, "next") || !strcmp(id, "prev")) {
        B.want = -1;
        if (*id == 'n') music_next();
        else music_prev();
        play(c, B.want >= 0 ? music_track(B.want)->path : "", 0);
    } else if (!strcmp(id, "favorite") || !strcmp(id, "mode")) {
        if (*id == 'm') {
            ink_args_get_str(&c->args, "mode", arg, sizeof arg);
            int m = 0;
            while (m < 4 && strcmp(arg, MODES[m])) m++;
            if (m < 4) music_set_mode((music_mode_t)m);
            else if (!strcmp(arg, "cycle")) music_cycle_mode();
            else return say(c, INK_RC_INVALID, TR_NOOP("没有这种循环方式"));
            snprintf(result, sizeof result, "{\"mode\":\"%s\"}", MODES[music_mode()]);
        } else {
            if (!path[0]) snprintf(path, sizeof path, "%s", B.st.path);
            if (!path[0]) return say(c, INK_RC_FAILED, TR_NOOP("还没有在放的歌"));
            const int index = music_find_path(path);
            ink_db_t *db = ink_db_open(&FAV, NULL);
            const bool now = index >= 0 ? !music_favorite(index) : !favorite_of(db, path);
            ink_db_close(db);
            if (index >= 0) music_favorite_toggle(index); /* it changes nothing when the collection refuses */
            if (index >= 0 ? music_favorite(index) != now : !music_port_favorite_store(path, now))
                return notice(TR_NOOP("收藏满了，先取消几首再收藏")), say(c, INK_RC_FAILED, TR_NOOP("收藏满了，先取消几首再收藏"));
            notice(NULL);
            snprintf(result, sizeof result, "{\"favorite\":%s}", now ? "true" : "false");
        }
        pick();
        ink_cap_result(c, result);
        draw(music_page_changed(id, path));
    } else if (!strcmp(id, "favorites")) {
        ink_db_t *db = ink_db_open(&FAV, NULL);
        ink_db_cursor_t cur = {0};
        ink_rec_t rec;
        ink_jw_t w;
        bool more = false;
        ink_jw_init(&w, result, sizeof result);
        ink_jw_raw(&w, "{\"items\":[");
        for (int n = 0; db && ink_db_next(db, &cur, &rec, path, sizeof path - 1) == 1 && !(more = w.len + 400 > sizeof result); n++) {
            path[rec.len < sizeof path - 1 ? rec.len : sizeof path - 1] = 0;
            const music_track_t *t = music_track(music_find_path(path));
            jfield(&w, n ? ",{\"path\":" : "{\"path\":", path); jfield(&w, ",\"title\":", t ? t->title : strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
            ink_jw_raw(&w, "}");
        }
        ink_db_close(db);
        ink_jw_raw(&w, more ? "],\"more\":true}" : "]}"); /* what did not fit: the AI pages with music.list */
        if (!w.overflow) ink_cap_result(c, result);
    } else { /* now */
        const int index = music_track_index();
        ink_jw_t w;
        ink_jw_init(&w, result, sizeof result);
        jfield(&w, "{\"path\":", B.st.path); jfield(&w, ",\"title\":", music_title()); jfield(&w, ",\"artist\":", music_artist());
        jfield(&w, ",\"album\":", B.st.path[0] ? B.album : ""); jfield(&w, ",\"mode\":", MODES[music_mode()]); jfield(&w, ",\"lyric\":", music_lyrics_at(B.st.position_ms));
        ink_jw_raw(&w, ",\"position_ms\":"); ink_jw_int(&w, B.st.position_ms); ink_jw_raw(&w, ",\"duration_ms\":"); ink_jw_int(&w, B.st.duration_ms);
        ink_jw_raw(&w, index >= 0 && music_favorite(index) ? ",\"favorite\":true" : ",\"favorite\":false");
        ink_jw_raw(&w, B.st.state == INK_MEDIA_PLAYING ? ",\"playing\":true}" : ",\"playing\":false}");
        if (!w.overflow) ink_cap_result(c, result);
    }
}
static void open_cap(ink_call_t *c) { (void)c; ink_ui_push(&PAGE, NULL); }

/* ---- the UI side: the graft client and the page ---- */
static void notices(void) { /* a missing font or an empty library is said on screen with the shared controls */
    const bool font = !V.font && U.active, empty = V.font && V.empty && U.active;
    if (font == U.drawn_font && empty == U.drawn_empty) return;
    for (int i = 0; i < 2; i++) {
        if (U.notice[i]) lv_obj_delete(U.notice[i]);
        U.notice[i] = NULL;
    }
    U.drawn_font = font; U.drawn_empty = empty;
    if (!U.screen || (!font && !empty)) return;
    U.notice[0] = ink_ui_empty(U.screen, font ? TR("缺少字体，播放页画不出来") : TR("还没有歌"), NULL, NULL, NULL);
    U.notice[1] = ink_ui_notice(U.screen, font ? TR("装机时会把 fonts/InkSC.ttf 放到卡上；现在可以让 AI 放歌") : TR("用传书传上来，或者放进 文件 › 音乐"),
                                INK_CANVAS_H / 2 + INK_CONTROL);
    ink_ui_content_changed();
}
static void presented_ui(void *arg) {
    (void)arg;
    if (U.active && ink_graft_current()) {
        V = B.view;
        if (B.font && !ink_graft_present(B.pixels, B.w, &B.shown, B.shown_refresh)) {
            static const char *const KIND[] = {"music.present.page", "music.present.whole", "music.present.part"};
            ink_probe_count(KIND[B.shown_refresh]);
            ink_probe_value("music.present_px", (double)((B.shown.x2 - B.shown.x1 + 1) * (B.shown.y2 - B.shown.y1 + 1)));
            ink_probe_value("music.present_y1", B.shown.y1);
            ink_probe_value("music.present_y2", B.shown.y2);
        }
        notices();
        ink_ui_content_changed();
    }
    __atomic_store_n(&presenting, false, __ATOMIC_RELEASE);
    ink_exec_post(INK_EXEC_BG, &after_job, after_present, NULL);
}
static void ticked(lv_timer_t *t) { (void)t; if (U.active) ink_exec_post(INK_EXEC_BG, &B.tick_job, tick_bg, NULL); }
static bool opened(void *ctx, const ink_member_identity_t *member, int w, int h) {
    (void)ctx;
    B.owner = *member;
    B.w = w;
    B.h = h;
    B.screen_w = ink_ui_width();
    B.percent = ink_ui_px(text_percent());
    U.active = true;
    return !ink_exec_post(INK_EXEC_BG, &B.open_job, opened_bg, NULL);
}
static void closed(void *ctx) {
    (void)ctx;
    U.active = false;
    memset(&V, 0, sizeof V);
    ink_exec_post(INK_EXEC_BG, &B.close_job, closed_bg, NULL);
}
static void paused(void *ctx) { (void)ctx; if (U.timer) lv_timer_pause(U.timer); }
static void resumed(void *ctx) { (void)ctx; if (U.timer) lv_timer_resume(U.timer); ticked(NULL); }
static bool input(void *ctx, const ink_graft_input_t *e) {
    (void)ctx;
    if (e->kind == INK_GRAFT_TAP) {
        B.tap_x = e->x;
        B.tap_y = e->y;
        return !ink_exec_post(INK_EXEC_BG, &B.tap_job, tap_bg, NULL);
    }
    int key = e->kind == INK_GRAFT_KEY && !e->held ? e->value : e->kind != INK_GRAFT_SWIPE ? -1
              : e->value == INK_SWIPE_LEFT ? INK_KEY_NEXT : e->value == INK_SWIPE_RIGHT ? INK_KEY_PREV : -1;
    if (key != INK_KEY_NEXT && key != INK_KEY_PREV && !(key == INK_KEY_BACK && V.all)) return false;
    B.key = key;
    return !ink_exec_post(INK_EXEC_BG, &B.key_job, key_bg, NULL);
}
static int snapshot(void *ctx, char *out, size_t cap) {
    (void)ctx;
    if (strcmp(V.owner, ink_member_current()->id)) return -1;
    static char rows[ROWS_MAX][96] INK_PSRAM_BSS, ids[ROWS_MAX][12] INK_PSRAM_BSS; /* UI task only */
    char page[48], library[96];
    snprintf(page, sizeof page, TR("第 %d 页，共 %d 页"), V.page + 1, V.pages);
    snprintf(library, sizeof library, V.limited ? TR("%d 首，只列出了前 200 首") : TR("%d 首"), V.count);
    ink_ui_field_spec_t f[11 + ROWS_MAX] = {
        {.id = "title", .label = TR("曲名"), .value = V.title}, {.id = "artist", .label = TR("歌手"), .value = V.artist},
        {.id = "album", .label = TR("专辑"), .value = V.album}, {.id = "progress", .label = TR("进度"), .value = V.progress},
        {.id = "state", .label = TR("播放"), .value = TR(V.state ? V.state : "")}, {.id = "mode", .label = TR("循环"), .value = TR(V.mode ? V.mode : "")},
        {.id = "lyrics", .label = TR("歌词"), .value = V.lyric}, {.id = "favorite", .label = TR("收藏"), .value = V.favorite ? TR("已收藏") : TR("没收藏")},
        {.id = "output", .label = TR("声音"), .value = TR(V.output ? V.output : "")}, {.id = "page", .label = V.all ? TR("全部歌曲") : TR("播放列表"), .value = page},
        {.id = "library", .label = TR("曲库"), .value = !V.font ? TR("缺少字体，播放页画不出来；可以让 AI 放歌") : V.empty ? TR("还没有歌。用传书传上来，或者放进 文件 › 音乐") : V.notice[0] ? V.notice : library}};
    for (int i = 0; i < 11; i++) f[i].role = "text";
    for (int i = 0; i < V.rows && i < ROWS_MAX; i++) {
        snprintf(ids[i], sizeof ids[i], "track%d", i);
        snprintf(rows[i], sizeof rows[i], "%s%s%s", V.row[i].name, V.row[i].detail[0] ? " · " : "", V.row[i].detail);
        f[11 + i] = (ink_ui_field_spec_t){.id = ids[i], .role = V.row[i].folder ? "directory" : "song", .label = rows[i], .value = V.row[i].path};
    }
    const ink_ui_action_spec_t actions[] = {
        {.id = "play", .label = TR("播放"), .capability = "music.play", .enabled = true}, {.id = "pause", .label = TR("暂停"), .capability = "music.pause", .enabled = true},
        {.id = "next", .label = TR("下一首"), .capability = "music.next", .enabled = true}, {.id = "prev", .label = TR("上一首"), .capability = "music.prev", .enabled = true},
        {.id = "seek", .label = TR("跳到指定位置"), .capability = "music.seek", .enabled = true}, {.id = "list", .label = TR("翻页、全部歌曲、文件夹"), .capability = "music.list", .enabled = true},
        {.id = "favorite", .label = TR("收藏或取消收藏"), .capability = "music.favorite", .enabled = true}, {.id = "mode", .label = TR("循环方式"), .capability = "music.mode", .enabled = true},
        {.id = "volume", .label = TR("音量"), .capability = "settings.set", .enabled = true}, {.id = "output", .label = TR("蓝牙耳机"), .capability = "shell.bluetooth", .enabled = true}};
    ink_ui_page_spec_t p = {.id = "Music", .title = TR("音乐"), .fields = f, .field_count = 11 + (size_t)(V.rows < ROWS_MAX ? V.rows : ROWS_MAX),
                            .actions = actions, .action_count = sizeof actions / sizeof *actions, .object = {"music", 1}};
    return ink_ui_semantics_snapshot(&p, out, cap);
}
static const ink_graft_client_t CLIENT = {.app = "music", .open = opened, .close = closed, .pause = paused, .resume = resumed, .input = input, .snapshot = snapshot};
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    U.screen = screen; U.drawn_font = U.drawn_empty = false;
    if (ink_graft_attach(screen, &CLIENT)) return;
    U.timer = lv_timer_create(ticked, 1000, NULL); /* only while the page is open and visible: progress and lyrics */
}
static void destroy(void) {
    if (U.timer) lv_timer_delete(U.timer);
    ink_graft_detach();
    U.timer = NULL; U.screen = U.notice[0] = U.notice[1] = NULL;
}
static const ink_activity_t PAGE = {.name = "Music", .app = "music", .create = create, .destroy = destroy, .on_key = ink_graft_key,
                                    .on_swipe = ink_graft_swipe, .settle = ink_graft_settle, .snapshot = ink_graft_snapshot};
static const ink_app_def_t APP = {.id = "music", .title = TR_NOOP("音乐"), .icon = "headphones", .entry = &PAGE, .system_app = true, .child_ok = true};
#define CAP(id_, title_, input_, effect_) {.id = id_, .title = title_, .app = "music", .input = input_, .effect = effect_, .child_ok = true, .timeout_ms = 30000, .exec = INK_EXEC_BG, .run = run}
static const ink_cap_t CAPS[] = {
    {.id = "music.open", .title = "打开音乐", .app = "music", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = open_cap},
    CAP("music.play", "播放某首、文件夹或继续播放", "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127}}}", INK_FX_NAVIGATE),
    CAP("music.pause", "暂停音乐", "{\"type\":\"object\"}", INK_FX_NAVIGATE), CAP("music.next", "下一首（按全部曲库）", "{\"type\":\"object\"}", INK_FX_NAVIGATE),
    CAP("music.prev", "上一首（按全部曲库）", "{\"type\":\"object\"}", INK_FX_NAVIGATE),
    CAP("music.seek", "跳到音乐的毫秒位置", "{\"type\":\"object\",\"properties\":{\"ms\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"ms\"]}", INK_FX_NAVIGATE),
    CAP("music.list", "音乐页翻页、全部歌曲和文件夹", "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127},\"page\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":10000},\"view\":{\"type\":\"string\",\"enum\":[\"player\",\"list\",\"all\"]}}}", INK_FX_READ),
    CAP("music.now", "正在播放的曲目、进度、循环方式、收藏和歌词", "{\"type\":\"object\"}", INK_FX_READ),
    CAP("music.favorite", "收藏或取消收藏一首歌（不给路径就是正在放的）", "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127}}}", INK_FX_LOCAL_WRITE),
    CAP("music.favorites", "收藏的歌", "{\"type\":\"object\"}", INK_FX_READ),
    CAP("music.mode", "循环方式：列表、单曲、随机、收藏循环，或 cycle 顺着切", "{\"type\":\"object\",\"properties\":{\"mode\":{\"type\":\"string\",\"enum\":[\"list\",\"one\",\"shuffle\",\"favorites\",\"cycle\"]}},\"required\":[\"mode\"]}", INK_FX_LOCAL_WRITE)};
int ink_music_app_init(void) {
    return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_cap_subscribe("media.finished", finished, NULL, INK_EXEC_BG) ||
           ink_state_watch("media.state", state_changed, NULL, INK_EXEC_BG) ||
           ink_cap_subscribe("file.changed", files_changed, NULL, INK_EXEC_BG) ? -1 : 0;
}
