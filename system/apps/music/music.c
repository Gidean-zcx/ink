/* ui-scale-runtime: dimension x,y,w Theme dimensions forwarded by button; callers use theme tokens. */
/* ui-scale-runtime: dimension progress_percent Dimensionless time ratio used to size the progress fill. */
/* kiikoread app_music.c layout: player, four-row playlist, folder browser. */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/audio.h"
#include "ink/sys/crc.h"
#include "ink/sys/file.h"
#include "ink/sys/layout.h"
#include "ink/sys/json.h"
#include "ink/sys/media.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/ui_shell.h"
#include "../../services/audio/vendor/kiikoread/music_lyrics.h"
void ink_music_tags(const char *, char *, char *, char *, size_t);
enum { ROWS = 4, FIELDS = ROWS + 5 };
typedef struct {
    char owner[INK_MEMBER_ID_MAX], dir[128], track[128], title[96], artist[96], album[96], lyric[192], time[48];
    char paths[ROWS][128], names[ROWS][96]; bool dirs[ROWS], favorite;
    int page, count, mode, volume; bool more, playlist; ink_media_status_t st;
} music_view_t;
static char result_json[2048] INK_PSRAM_BSS;
static music_view_t B INK_PSRAM_BSS, V INK_PSRAM_BSS; /* BG and UI ownership, respectively. */
static const ink_db_def_t FAV = {.name = "music.favorites", .title = "收藏的歌曲", .scope = INK_DB_MEMBER, .schema = 1, .ai_read = true, .max_records = 100};
static const ink_db_def_t LAST = {.name = "music.last", .title = "上次听的歌曲", .scope = INK_DB_MEMBER, .schema = 1, .max_records = 1};
static lv_obj_t *title_label, *time_label, *lyric_label, *play_button, *progress_fill;
static lv_timer_t *timer;
static const ink_activity_t PAGE;
static uint32_t favorite_id(const char *p) { return (ink_crc32(0, p, strlen(p)) & 0x7fffffff) + 1; }
static bool supported(const char *p) {
    const char *e = strrchr(p, '.'); const char *const exts[] = {".mp3", ".flac", ".wav", ".ogg", ".oga", ".opus"};
    for (unsigned i = 0; e && i < sizeof exts / sizeof *exts; i++) if (!strcasecmp(e, exts[i])) return true;
    return false;
}
static void member(void) {
    const char *id = ink_member_current()->id;
    if (strcmp(B.owner, id)) { memset(&B, 0, sizeof B); snprintf(B.owner, sizeof B.owner, "%s", id); snprintf(B.dir, sizeof B.dir, "/me/音乐"); music_lyrics_clear(); }
}
static void last_path(bool save, char path[128]) {
    ink_db_t *db = ink_db_open(&LAST, NULL); ink_rec_t rec;
    ink_db_write_t w = {.id = 1, .data = path, .len = strlen(path) + 1, .src = INK_SRC_SYSTEM};
    if (db && save) ink_db_put(db, &w, NULL);
    if (db && !save && ink_db_get(db, 1, &rec, path, 127) == INK_DB_OK) path[rec.len < 127 ? rec.len : 127] = 0;
    ink_db_close(db);
}
static void refresh_now(void) {
    ink_media_status(&B.st); B.volume = ink_audio_effective_volume();
    if (strcmp(B.track, B.st.path)) {
        snprintf(B.track, sizeof B.track, "%s", B.st.path); B.title[0] = B.artist[0] = B.album[0] = 0; B.playlist = false; music_lyrics_clear();
        if (B.track[0]) { ink_music_tags(B.track, B.title, B.artist, B.album, sizeof B.title); music_lyrics_load(B.track, B.st.duration_ms); }
    }
    const char *s = music_lyrics_at(B.st.position_ms); size_t n = strlen(s);
    if (n >= sizeof B.lyric) { n = sizeof B.lyric - 1; while (n && ((unsigned char)s[n] & 0xc0) == 0x80) n--; }
    memcpy(B.lyric, s, n); B.lyric[n] = 0;
    snprintf(B.time, sizeof B.time, "%u:%02u / %u:%02u", (unsigned)(B.st.position_ms / 60000), (unsigned)(B.st.position_ms / 1000 % 60), (unsigned)(B.st.duration_ms / 60000), (unsigned)(B.st.duration_ms / 1000 % 60));
    ink_db_t *db = ink_db_open(&FAV, NULL); ink_rec_t rec; char path[128];
    B.favorite = db && B.track[0] && ink_db_get(db, favorite_id(B.track), &rec, path, sizeof path) == INK_DB_OK && !strcmp(path, B.track);
    ink_db_close(db);
}
typedef struct { ink_exec_job_t job; music_view_t data; } delivery_t;
static void show_values(void) {
    if (title_label) lv_label_set_text(title_label, V.title[0] ? V.title : "还没有播放歌曲");
    if (time_label) lv_label_set_text(time_label, V.time);
    if (lyric_label) lv_label_set_text(lyric_label, V.lyric[0] ? V.lyric : "暂无歌词");
    unsigned progress_percent = V.st.duration_ms ? (unsigned)((uint64_t)V.st.position_ms * 100 / V.st.duration_ms) : 0;
    if (progress_fill) lv_obj_set_width(progress_fill, ink_ui_px((int)((INK_CANVAS_W - 2 * INK_MARGIN) * progress_percent / 100)));
    if (play_button) {
        lv_label_set_text(lv_obj_get_child(play_button, 0), V.st.state == INK_MEDIA_PLAYING ? "暂停" : "播放");
        ink_shell_bind(play_button, V.st.state == INK_MEDIA_PLAYING ? "music.pause" : "music.play", "{}");
    }
}
static void delivered(void *arg) {
    delivery_t *d = arg;
    if (!strcmp(d->data.owner, ink_member_current()->id)) {
        bool rebuild = V.mode != d->data.mode || V.page != d->data.page || V.volume != d->data.volume || strcmp(V.dir, d->data.dir) || V.favorite != d->data.favorite || memcmp(V.paths, d->data.paths, sizeof V.paths);
        bool changed = memcmp(&V, &d->data, sizeof V) != 0; V = d->data;
        if (!strcmp(ink_ui_page_name(), "Music") && changed) { if (rebuild) ink_ui_redraw(); else { show_values(); ink_ui_content_changed(); } }
    }
    osal_free(d);
}
static void publish(void) {
    delivery_t *d = osal_calloc(1, sizeof *d, OSAL_MEM_EXTERNAL);
    if (d) { d->data = B; if (ink_exec_post(INK_EXEC_UI, &d->job, delivered, d)) osal_free(d); }
}
static void list(ink_call_t *c) {
    char path[128] = "", mode[16] = ""; int64_t page = B.page;
    ink_args_get_str(&c->args, "path", path, sizeof path); ink_args_get_str(&c->args, "view", mode, sizeof mode); ink_args_get_int(&c->args, "page", &page);
    if (path[0]) { snprintf(B.dir, sizeof B.dir, "%s", path); page = 0; }
    if (mode[0]) B.mode = !strcmp(mode, "all") ? 2 : !strcmp(mode, "list") ? 1 : 0;
    ink_fs_dir_t *dir = NULL; int e = ink_fs_dir_open(B.dir, &dir);
    if (e) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "请把音乐放到文件中的“音乐”文件夹"); return; }
    B.page = page < 0 ? 0 : (int)page; B.count = 0; B.more = false; memset(B.paths, 0, sizeof B.paths);
    char directory[128]; snprintf(directory, sizeof directory, "%s", B.dir);
    ink_fs_stat_t entry; int skip = B.page * ROWS, n;
    while ((n = ink_fs_dir_next(dir, &entry)) == 1) {
        if ((!entry.is_dir && !supported(entry.name)) || (entry.is_dir && B.mode != 2)) continue;
        if (skip > 0) { skip--; continue; }
        if (B.count == ROWS) { B.more = true; break; }
        int i = B.count; int len = snprintf(B.paths[i], sizeof B.paths[i], "%s/%s", directory, entry.name);
        if (len < 0 || len >= (int)sizeof B.paths[i]) continue;
        B.dirs[i] = entry.is_dir; snprintf(B.names[i], sizeof B.names[i], "%s", entry.name);
        if (!entry.is_dir) { char artist[96], album[96]; ink_music_tags(B.paths[i], B.names[i], artist, album, sizeof artist); }
        B.count++;
    }
    ink_fs_dir_close(dir);
    if (n < 0) c->rc.status = INK_RC_FAILED;
    ink_jw_t w; ink_jw_init(&w, result_json, sizeof result_json); ink_jw_raw(&w, "{\"path\":"); ink_jw_str(&w, B.dir); ink_jw_raw(&w, ",\"items\":[");
    for (int i = 0; i < B.count; i++) { if (i) ink_jw_raw(&w, ",");
        ink_jw_raw(&w, "{\"path\":"); ink_jw_str(&w, B.paths[i]); ink_jw_raw(&w, ",\"title\":"); ink_jw_str(&w, B.names[i]); ink_jw_raw(&w, B.dirs[i] ? ",\"directory\":true}" : ",\"directory\":false}"); }
    ink_jw_raw(&w, B.more ? "],\"more\":true}" : "],\"more\":false}");
    if (!w.overflow) ink_cap_result(c, result_json);
}
static void forwarded(void *ctx, const ink_receipt_t *rc) {
    ink_call_t *c = ctx; c->rc.status = rc->status; snprintf(c->rc.say, sizeof c->rc.say, "%s", rc->say); snprintf(c->rc.detail, sizeof c->rc.detail, "%s", rc->detail);
    if (rc->result) ink_cap_result(c, rc->result);
    bool played = rc->status == INK_RC_DONE && (!strcmp(c->cap->id, "music.play") || !strcmp(c->cap->id, "music.next") || !strcmp(c->cap->id, "music.prev"));
    ink_cap_done(c); member(); refresh_now();
    if (played && B.track[0]) { B.playlist = true; last_path(true, B.track); }
    publish();
}
static void run(ink_call_t *c) {
    member(); const char *id = c->cap->id;
    if (!strcmp(id, "music.list")) { list(c); refresh_now(); publish(); return; }
    refresh_now();
    if (!strcmp(id, "music.now")) {
        ink_jw_t w; ink_jw_init(&w, result_json, sizeof result_json);
        ink_jw_raw(&w, "{\"path\":"); ink_jw_str(&w, B.track); ink_jw_raw(&w, ",\"title\":"); ink_jw_str(&w, B.title); ink_jw_raw(&w, ",\"artist\":"); ink_jw_str(&w, B.artist); ink_jw_raw(&w, ",\"album\":"); ink_jw_str(&w, B.album);
        ink_jw_raw(&w, ",\"position_ms\":"); ink_jw_int(&w, B.st.position_ms); ink_jw_raw(&w, ",\"duration_ms\":"); ink_jw_int(&w, B.st.duration_ms); ink_jw_raw(&w, B.st.state == INK_MEDIA_PLAYING ? ",\"playing\":true}" : ",\"playing\":false}");
        if (!w.overflow) ink_cap_result(c, result_json);
        publish(); return;
    }
    if (!strcmp(id, "music.favorite")) {
        ink_db_t *db = ink_db_open(&FAV, NULL); ink_db_write_t w = {.id = favorite_id(B.track), .data = B.track, .len = strlen(B.track) + 1, .src = c->src};
        if (!db || !B.track[0] || (B.favorite ? ink_db_del(db, w.id, c->src) : ink_db_put(db, &w, NULL)) != INK_DB_OK) c->rc.status = INK_RC_FAILED;
        ink_db_close(db); refresh_now(); publish(); return;
    }
    ink_args_t a; ink_args_init(&a); const char *target = !strcmp(id, "music.pause") ? "media.pause" : !strcmp(id, "music.seek") ? "media.seek" : "media.play";
    if (!strcmp(id, "music.seek")) a = c->args;
    else if (strcmp(id, "music.pause")) {
        char path[128] = ""; ink_args_get_str(&c->args, "path", path, sizeof path);
        bool step = !strcmp(id, "music.next") || !strcmp(id, "music.prev");
        if (!path[0] && !step && B.st.state == INK_MEDIA_PAUSED) target = "media.resume";
        else {
            if (!path[0] && !step) { snprintf(path, sizeof path, "%s", B.track); if (!path[0]) last_path(false, path); }
            ink_fs_stat_t st; bool folder = path[0] && !ink_fs_stat(path, &st) && st.is_dir;
            if (folder) { snprintf(B.dir, sizeof B.dir, "%s", path); path[0] = 0; }
            if (step || folder || !path[0]) {
                uint32_t cursor = 0; ink_fs_entry_t entry; char first[128] = "", prev[128] = ""; bool found = false; int n;
                while ((n = ink_fs_index(B.dir, INK_FS_AUDIO, &cursor, &entry)) == 1) {
                    if (!supported(entry.path)) continue;
                    if (!first[0]) snprintf(first, sizeof first, "%s", entry.path);
                    if (!step || (found && strcmp(id, "music.prev"))) { snprintf(path, sizeof path, "%s", entry.path); break; }
                    if (!strcmp(entry.path, B.track)) { found = true; if (!strcmp(id, "music.prev") && prev[0]) { snprintf(path, sizeof path, "%s", prev); break; } }
                    snprintf(prev, sizeof prev, "%s", entry.path);
                }
                if (!path[0]) snprintf(path, sizeof path, "%s", !strcmp(id, "music.prev") && prev[0] ? prev : first);
            }
            if (!supported(path)) { c->rc.status = INK_RC_INVALID; return; }
            if (!step && !folder) { snprintf(B.dir, sizeof B.dir, "%s", path); char *slash = strrchr(B.dir, '/'); if (slash) *slash = 0; }
            ink_args_str(&a, "path", path);
        }
    }
    ink_call_opts_t o = {.src = c->src, .done = forwarded, .ctx = c, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_BG};
    ink_cap_defer(c);
    if (ink_cap_call(target, &a, &o) < 0) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
}
static void open_cap(ink_call_t *c) { (void)c; ink_ui_push(&PAGE, NULL); }
static void poll(lv_timer_t *t) { (void)t; ink_call_opts_t o = {.src = INK_SRC_SYSTEM}; ink_cap_call("music.now", NULL, &o); }
static void button(lv_obj_t *s, const char *label, const char *cap, const char *args, int x, int y, int w) {
    lv_obj_t *b = ink_ui_pill(s, label, x, y, w, INK_CONTROL, false, NULL, NULL); ink_shell_bind(b, cap, args);
}
static void volume_done(void *ctx, const ink_receipt_t *rc) { (void)ctx; (void)rc; poll(NULL); }
static void slider(lv_event_t *e) {
    lv_indev_t *in = lv_event_get_indev(e); if (!in) return;
    lv_point_t p; lv_indev_get_point(in, &p); lv_area_t box; lv_obj_get_coords(lv_event_get_target_obj(e), &box);
    int span = box.x2 - box.x1, v = span > 0 ? (p.x - box.x1) * 100 / span : 0; if (v < 0) v = 0;
    if (v > 100) v = 100;
    ink_args_t a; ink_args_init(&a); bool seek = lv_event_get_user_data(e) != NULL;
    if (seek) ink_args_int(&a, "ms", (int64_t)V.st.duration_ms * v / 100);
    else { ink_args_str(&a, "id", "audio.volume"); ink_args_int(&a, "value", v); }
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .done = volume_done, .reply_exec = INK_EXEC_UI}; ink_cap_call(seek ? "music.seek" : "settings.set", &a, &o);
}
static void create(lv_obj_t *s, void *arg) {
    (void)arg; if (strcmp(V.owner, ink_member_current()->id)) memset(&V, 0, sizeof V);
    ink_shell_statusbar(s, true); ink_shell_topbar(s, "音乐"); int w = INK_CANVAS_W - 2 * INK_MARGIN, y = INK_STATUS_BAR + INK_CONTROL + INK_GAP;
    button(s, "播放页", "music.list", "{\"view\":\"player\",\"page\":0}", INK_MARGIN, y, w / 3 - INK_GAP_S);
    button(s, "播放列表", "music.list", "{\"view\":\"list\",\"page\":0}", INK_MARGIN + w / 3, y, w / 3 - INK_GAP_S);
    button(s, "全部歌曲", "music.list", "{\"view\":\"all\",\"page\":0}", INK_MARGIN + 2 * w / 3, y, w / 3); y += INK_CONTROL + INK_GAP;
    if (!V.mode) {
        title_label = ink_ui_line(s, "", INK_FONT_SUBTITLE, INK_WEIGHT_MEDIUM, INK_MARGIN, y, w); y += ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP;
        time_label = ink_ui_line(s, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w); y += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP;
        lv_obj_t *bar = ink_ui_box(s, INK_MARGIN, y, w, INK_GAP_L, 0, INK_SURFACE_STATIC, false);
        progress_fill = ink_ui_box(bar, 0, 0, 1, INK_GAP_L, 0, INK_SURFACE_BLACK, false); lv_obj_add_flag(bar, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(bar, slider, LV_EVENT_RELEASED, (void *)1); ink_ui_capture_touch(bar); y += INK_GAP_L + INK_GAP;
        lyric_label = ink_ui_line(s, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w); y += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP;
        button(s, "上一首", "music.prev", "{}", INK_MARGIN, y, w / 3 - INK_GAP_S);
        play_button = ink_ui_pill(s, "播放", INK_MARGIN + w / 3, y, w / 3 - INK_GAP_S, INK_CONTROL, true, NULL, NULL);
        button(s, "下一首", "music.next", "{}", INK_MARGIN + 2 * w / 3, y, w / 3); y += INK_CONTROL + INK_GAP;
        button(s, V.favorite ? "取消收藏" : "收藏", "music.favorite", "{}", INK_MARGIN, y, w / 2 - INK_GAP_S);
        char json[48]; snprintf(json, sizeof json, "{\"ms\":%u}", (unsigned)(V.st.position_ms + 10000)); button(s, "快进10秒", "music.seek", json, INK_MARGIN + w / 2, y, w / 2); y += INK_CONTROL + INK_GAP;
        ink_ui_slider(s, "音量", V.volume / 10, INK_MARGIN, y, w, slider, NULL); y += INK_SLIDER + INK_GAP;

    } else {
        ink_ui_line(s, V.dir[0] ? V.dir : "/me/音乐", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w); y += INK_CONTROL;
        for (int i = 0; i < V.count; i++) {
            char json[180]; ink_jw_t jw; ink_jw_init(&jw, json, sizeof json); ink_jw_raw(&jw, "{\"path\":"); ink_jw_str(&jw, V.paths[i]); ink_jw_raw(&jw, "}");
            lv_obj_t *row = ink_ui_list_row(s, V.dirs[i] ? "folder" : "headphones", V.names[i], V.dirs[i] ? "文件夹" : "播放", INK_MARGIN, y, w, true, NULL, NULL);
            ink_shell_bind(row, V.dirs[i] ? "music.list" : "music.play", json); y += ink_ui_row_double() + INK_GAP_S;
        }
        char json[180], parent[128]; snprintf(parent, sizeof parent, "%s", V.dir); char *slash = strrchr(parent, '/'); if (slash && slash > parent + 3) *slash = 0;
        ink_jw_t jw; ink_jw_init(&jw, json, sizeof json); ink_jw_raw(&jw, "{\"path\":"); ink_jw_str(&jw, parent); ink_jw_raw(&jw, "}");
        button(s, "上级目录", "music.list", json, INK_MARGIN, y, w); y += INK_CONTROL + INK_GAP;
        snprintf(json, sizeof json, "{\"page\":%d}", V.page > 0 ? V.page - 1 : 0); button(s, "上一页", "music.list", json, INK_MARGIN, y, w / 2 - INK_GAP_S);
        snprintf(json, sizeof json, "{\"page\":%d}", V.page + (V.more ? 1 : 0)); button(s, "下一页", "music.list", json, INK_MARGIN + w / 2, y, w / 2);
    }
    show_values(); timer = lv_timer_create(poll, 5000, NULL); poll(NULL);
}
static void destroy(void) { if (timer) lv_timer_delete(timer); timer = NULL; title_label = time_label = lyric_label = play_button = progress_fill = NULL; }
static int snapshot(char *out, size_t cap) {
    if (strcmp(V.owner, ink_member_current()->id)) { if (cap) out[0] = 0;
        return -1; }
    ink_ui_field_spec_t f[FIELDS] = {{.id = "title", .label = "曲名", .value = V.title}, {.id = "progress", .label = "进度", .value = V.time}, {.id = "lyrics", .label = "歌词", .value = V.lyric}, {.id = "artist", .label = "歌手", .value = V.artist}, {.id = "album", .label = "专辑", .value = V.album}};
    for (int i = 0; i < 5; i++) f[i].role = "text";
    char ids[ROWS][16]; for (int i = 0; i < V.count; i++) { snprintf(ids[i], sizeof ids[i], "track%d", i); f[5 + i] = (ink_ui_field_spec_t){.id = ids[i], .role = V.dirs[i] ? "directory" : "song", .label = V.names[i], .value = V.paths[i]}; }
    static const ink_ui_action_spec_t actions[] = {
        {.id = "play", .label = "播放", .capability = "music.play", .enabled = true}, {.id = "pause", .label = "暂停", .capability = "music.pause", .enabled = true},
        {.id = "next", .label = "下一首", .capability = "music.next", .enabled = true}, {.id = "prev", .label = "上一首", .capability = "music.prev", .enabled = true},
        {.id = "seek", .label = "跳到指定位置", .capability = "music.seek", .enabled = true}, {.id = "list", .label = "歌曲和文件夹", .capability = "music.list", .enabled = true},
        {.id = "favorite", .label = "收藏或取消收藏", .capability = "music.favorite", .enabled = true}, {.id = "volume", .label = "音量", .capability = "settings.set", .enabled = true}};
    _Static_assert(5 + ROWS <= FIELDS, "music field capacity");
    ink_ui_page_spec_t p = {.id = "Music", .title = "音乐", .fields = f, .field_count = 5 + (size_t)V.count, .actions = actions, .action_count = sizeof actions / sizeof *actions, .object = {"music", 1}}; return ink_ui_semantics_snapshot(&p, out, cap);
}
static const ink_activity_t PAGE = {.name = "Music", .app = "music", .create = create, .destroy = destroy, .snapshot = snapshot};
static const ink_app_def_t APP = {.id = "music", .title = "音乐", .icon = "headphones", .entry = &PAGE, .system_app = true, .child_ok = true};
#define CAP(id_, title_, input_, effect_) {.id = id_, .title = title_, .app = "music", .input = input_, .effect = effect_, .child_ok = true, .exec = INK_EXEC_BG, .run = run}
static const ink_cap_t CAPS[] = {
    {.id = "music.open", .title = "打开音乐", .app = "music", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = open_cap},
    CAP("music.play", "播放某首、文件夹或继续播放", "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127}}}", INK_FX_NAVIGATE),
    CAP("music.pause", "暂停音乐", "{\"type\":\"object\"}", INK_FX_NAVIGATE), CAP("music.next", "下一首", "{\"type\":\"object\"}", INK_FX_NAVIGATE), CAP("music.prev", "上一首", "{\"type\":\"object\"}", INK_FX_NAVIGATE),
    CAP("music.seek", "跳到音乐的毫秒位置", "{\"type\":\"object\",\"properties\":{\"ms\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"ms\"]}", INK_FX_NAVIGATE),
    CAP("music.list", "音乐列表和文件夹", "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127},\"page\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":10000},\"view\":{\"type\":\"string\",\"enum\":[\"player\",\"list\",\"all\"]}}}", INK_FX_READ),
    CAP("music.now", "正在播放的曲目和进度", "{\"type\":\"object\"}", INK_FX_READ), CAP("music.favorite", "收藏或取消收藏当前歌曲", "{\"type\":\"object\"}", INK_FX_LOCAL_WRITE)};
static void finished(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id; char why[24], path[128]; member();
    ink_args_get_str(a, "why", why, sizeof why); ink_args_get_str(a, "path", path, sizeof path);
    if (B.playlist && !strcmp(why, "finished") && !strcmp(path, B.track)) { ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = B.owner}; ink_cap_call("music.next", NULL, &o); }
}
int ink_music_init(void) { return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_cap_subscribe("media.finished", finished, NULL, INK_EXEC_BG) ? -1 : 0; }
