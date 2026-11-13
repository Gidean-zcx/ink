/* Gallery (gallery-3): photos by date and album, one at a time with zoom and turning, their details, and
 * 最近删除 to get a deleted one back. Rewritten after Aves and Fossify Gallery for this panel
 * (docs/tasks/gallery-3/对照表.md); the pure helpers are AgentUI's (repository.cpp). */
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/appreg.h"
#include "ink/sys/i18n.h"
#include "ink/sys/file.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "ink/sys/time.h"
#include "ink/sys/ai.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include "photo.h"
#include "repository.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { lv_image_dsc_t dsc; ink_image_t im; } picture_t;
static void picture_deleted(lv_event_t *e) {
    picture_t *p = lv_event_get_user_data(e); ink_image_free(&p->im); osal_free(p);
}
void ink_photo_image(lv_obj_t *parent, ink_image_t *im) {
    picture_t *p = osal_calloc(1, sizeof *p, OSAL_MEM_EXTERNAL);
    if (!p) return;
    p->im = *im; *im = (ink_image_t){0};
    p->dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_L8,
        .w = p->im.width, .h = p->im.height, .stride = p->im.width}, .data_size = p->im.width * p->im.height, .data = p->im.pixels};
    lv_obj_t *obj = lv_image_create(parent); lv_image_set_src(obj, &p->dsc);
    lv_obj_update_layout(parent);
    unsigned sx = (unsigned)lv_obj_get_content_width(parent) * LV_SCALE_NONE / p->im.width;
    unsigned sy = (unsigned)lv_obj_get_content_height(parent) * LV_SCALE_NONE / p->im.height;
    lv_image_set_scale(obj, sx < sy ? sx : sy); lv_image_set_antialias(obj, false); lv_obj_center(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(obj, picture_deleted, LV_EVENT_DELETE, p);
}

static const ink_activity_t GALLERY, PHOTO, INFO, TRASH;
enum { PHOTO_SLOTS = 9, PHOTO_COLUMNS = 3, LIST_SLOTS = 16, ALBUM_MAX = 64, MONTH_MAX = 120, INFO_ROWS = 7, TRASH_DAYS = 30 };
enum { VIEW_ALL, VIEW_ALBUMS, VIEW_MONTHS };
enum { FILTER_NONE, FILTER_ALBUM, FILTER_MONTH };
enum { SEEN_OK, SEEN_TOO_BIG, SEEN_FORMAT, SEEN_BROKEN };
/* Fixed folders on the card, like the camera's: translating them would change where the photos are. */
static const char TRASH_DIR[] = "最近删除", UNDATED_DIR[] = "日期不详", LOCK_ALBUM[] = "/family/锁屏拍的照片";
#define EARLIEST_DATED 1577836800 /* 2020-01-01: older file times come from a clock that was never set */
#define UNDO_MS 60000
enum { GALLERY_BATCH_MS = 1500,     /* a page's thumbnails are drawn together, or after this */
       GALLERY_DOUBLE_TAP_MS = 400 }; /* [估算] two taps closer than this are a double tap */

typedef struct { char path[INK_FS_PATH_MAX]; uint16_t count; int64_t newest; } album_t;
typedef struct { char key[8]; uint16_t begin, count; } month_t;
typedef struct { /* one reading of the card, in PSRAM; the UI keeps the newest */
    ink_gallery_item_t items[INK_FS_INDEX_MAX], trash[INK_FS_INDEX_MAX];
    char paths[INK_FS_INDEX_MAX][INK_FS_PATH_MAX];
    album_t albums[ALBUM_MAX]; month_t months[MONTH_MAX];
    size_t count, ntrash, nalbums, nmonths, used;
    int err;
} photos_t;
typedef struct { int zoom, rot; float cx, cy; unsigned w, h; } view_t; /* cx, cy: centre in upright, turned picture pixels */
typedef struct { uint32_t epoch, generation, photo, revision; } photo_question_t;
typedef struct { const char *icon, *label, *cap, *args; } tool_t;
static struct {
    ink_ui_list_t *list; ink_ui_list_position_t position, rows, trash_position, info_position;
    photos_t *data;
    uint32_t ids[INK_FS_INDEX_MAX], epoch, photo, revision, last_photo, batch, seq, view_photo, opened;
    size_t count;
    int view, filter, seen, layout; bool fresh, waiting; uint16_t album; char month[8];
    lv_obj_t *slots[PHOTO_SLOTS], *box, *status, *name, *body, *content, *image, *segments, *month_line;
    char objects[LIST_SLOTS][32], args[LIST_SLOTS][INK_FS_PATH_MAX + 32], labels[LIST_SLOTS][96], values[LIST_SLOTS][96];
    char path[INK_FS_PATH_MAX], message[160], caption[INK_FS_PATH_MAX + 64], line[96], trashing[INK_FS_PATH_MAX], notice[160];
    char undo_args[INK_FS_PATH_MAX + 32], forever_args[INK_FS_PATH_MAX + 32], info_values[INFO_ROWS][96];
    const char *info_labels[INFO_ROWS]; size_t ninfo;
    unsigned expected, arrived; int8_t thumb_state[PHOTO_SLOTS]; ink_image_t thumbs[PHOTO_SLOTS];
    ink_svc_deadline_t batch_at; ink_exec_job_t batch_job;
    view_t v; unsigned dw, dh; ink_image_t shown; lv_image_dsc_t dsc;
    ink_image_info_t info; int64_t size, mtime; uint64_t tap_us; lv_point_t tap_at, press_at;
    ink_ui_action_spec_t photo_actions[16]; size_t nactions;
} G INK_PSRAM_BSS;
_Static_assert(PHOTO_SLOTS <= LIST_SLOTS, "grid slots share the list buffers");

/* ------------------------------------------------------------------ names, dates, places */

static const char *base_name(const char *path) { const char *s = strrchr(path, '/'); return s ? s + 1 : path; }
static bool under(const char *path, const char *dir) { size_t n = strlen(dir); return !strncmp(path, dir, n) && path[n] == '/'; }
static void trash_root(const char *space, char *out, size_t cap) { snprintf(out, cap, "%s/%s", space, TRASH_DIR); }
static bool in_trash(const char *path) {
    char me[32], family[40]; trash_root("/me", me, sizeof me); trash_root("/family", family, sizeof family);
    return under(path, me) || under(path, family);
}
static bool is_image(const char *path) {
    const char *ext = strrchr(path, '.'); char low[8] = "";
    for (int i = 0; ext && ext[i] && i < 7; i++) low[i] = (char)(ext[i] >= 'A' && ext[i] <= 'Z' ? ext[i] + 32 : ext[i]);
    return ext && strlen(ext) < 7 && strstr(".png.jpg.jpeg.bmp.gif.webp.", low) && strstr(".png.jpg.jpeg.bmp.gif.webp.", low)[strlen(low)] == '.';
}
static bool dated(int64_t utc_s) { return utc_s >= EARLIEST_DATED; }
static int this_year(void) { ink_tm_t t; if (!ink_time_valid()) return 0; ink_time_to_local(ink_time_now_s(), &t); return t.year; }
static void month_label(const char *key, char *out, size_t cap) {
    int y = 0, m = 0;
    if (sscanf(key, "%4d-%2d", &y, &m) == 2) snprintf(out, cap, TR("%d年%d月"), y, m);
    else snprintf(out, cap, "%s", TR("日期不详"));
}
static void day_text(int64_t utc_s, char *out, size_t cap) { /* "11月13日", the year in front when not this one */
    if (!dated(utc_s)) { snprintf(out, cap, "%s", TR("日期不详")); return; }
    char day[32]; ink_gallery_day_label(utc_s, day, sizeof day);
    ink_tm_t t; ink_time_to_local(utc_s, &t);
    if (t.year != this_year()) snprintf(out, cap, TR("%d年%s"), t.year, day); else snprintf(out, cap, "%s", day);
}
static const char *album_name(const char *dir) {
    return !strcmp(dir, "/me") || !strcmp(dir, "/family") ? TR("其他照片") : base_name(dir);
}
static ink_gallery_item_t *item_of(uint32_t id) {
    for (size_t i = 0; G.data && i < G.data->count; i++) if (G.data->items[i].id == id) return &G.data->items[i];
    return NULL;
}
static size_t index_of(uint32_t id) { size_t i = 0; while (i < G.count && G.ids[i] != id) i++; return i; }
/* A photo in 最近删除: <space>/最近删除/<day>/<where it was>. */
static bool trash_parts(const char *path, char *day, size_t day_cap, char *origin, size_t origin_cap) {
    const char *space = !strncmp(path, "/me/", 4) ? "/me" : "/family";
    char root[40]; trash_root(space, root, sizeof root);
    if (!under(path, root)) return false;
    const char *d = path + strlen(root) + 1, *rest = strchr(d, '/');
    if (!rest || rest == d || !rest[1]) return false;
    snprintf(day, day_cap, "%.*s", (int)(rest - d), d);
    return snprintf(origin, origin_cap, "%s%s", space, rest) < (int)origin_cap;
}
static int days_since(const char *day) { /* -1: not a date. Whether the clock can be trusted is the caller's question. */
    ink_tm_t t = {0}, now;
    if (sscanf(day, "%4d-%2d-%2d", &t.year, &t.month, &t.day) != 3) return -1;
    ink_time_to_local(osal_wall_us() / 1000000, &now); now.hour = now.min = now.sec = 0;
    return (int)((ink_time_from_local(&now) - ink_time_from_local(&t) + 43200) / 86400);
}

/* ------------------------------------------------------------------ reading the card (media executor) */

typedef struct {
    ink_exec_job_t job; ink_svc_deadline_t retry;
    uint32_t epoch, generation, id, revision, seq, open_id; int err, waits, seen; bool adult;
    photos_t *data; ink_image_t im; ink_image_info_t info; int64_t size, mtime; view_t v; unsigned dw, dh;
    char path[INK_FS_PATH_MAX];
} load_t;
static ink_image_t M_ORIGINAL INK_PSRAM_BSS; /* media executor only: the shown photo, upright, not dithered */

static void add_photo(photos_t *d, const ink_fs_entry_t *e, bool trash) {
    ink_gallery_item_t *list = trash ? d->trash : d->items; size_t *n = trash ? &d->ntrash : &d->count;
    if (d->used >= INK_FS_INDEX_MAX) return; /* the card's index holds at most this many files of every kind */
    char *path = d->paths[d->used++]; snprintf(path, INK_FS_PATH_MAX, "%s", e->path);
    ink_gallery_item_t *it = &list[(*n)++];
    *it = (ink_gallery_item_t){.id = e->id, .modified = e->mtime >= 0 ? e->mtime / 1000000000 : -1, .name = base_name(path), .path = path, .size = e->size};
    if (!dated(it->modified)) snprintf(it->month, sizeof it->month, "undated");
    else { char key[16]; ink_gallery_date_key(it->modified, '-', key, sizeof key); snprintf(it->month, sizeof it->month, "%.7s", key); }
}
static void scan(photos_t *d, const char *root, uint32_t epoch, bool trash_too) {
    uint32_t cursor = 0; ink_fs_entry_t e; int r;
    while ((r = ink_fs_index(root, INK_FS_IMAGE, &cursor, &e)) == 1 && epoch == ink_member_epoch()) {
        bool trash = in_trash(e.path);
        if (!trash || trash_too) add_photo(d, &e, trash);
    }
    if (r < 0 && r != INK_FS_ERR_NOT_FOUND && !d->err) d->err = r;
}
static void month_row(void *ctx, size_t begin, size_t end) {
    photos_t *d = ctx;
    if (d->nmonths < MONTH_MAX) {
        month_t *m = &d->months[d->nmonths++]; memcpy(m->key, d->items[begin].month, sizeof m->key);
        m->begin = (uint16_t)begin; m->count = (uint16_t)(end - begin);
    }
}
static void organize(photos_t *d) {
    ink_gallery_sort(d->items, d->count);
    for (size_t i = 0; i < d->count; i++) {
        char dir[INK_FS_PATH_MAX]; snprintf(dir, sizeof dir, "%.*s", (int)(d->items[i].name - 1 - d->items[i].path), d->items[i].path);
        size_t a = 0; while (a < d->nalbums && strcmp(d->albums[a].path, dir)) a++;
        if (a == d->nalbums && a < ALBUM_MAX) { snprintf(d->albums[a].path, sizeof d->albums[a].path, "%s", dir); d->albums[a].newest = d->items[i].modified; d->nalbums++; }
        d->items[i].album = a < d->nalbums ? (uint16_t)a : UINT16_MAX;
        if (a < d->nalbums) d->albums[a].count++;
    }
    ink_gallery_month_groups(d->items, d->count, month_row, d);
}
static void photo_seen(load_t *p, const ink_fs_entry_t *e) { /* only what the file's head says: why it cannot be shown */
    ink_fs_file_t *f = NULL; size_t want = e->size < INK_IMAGE_PROBE_BYTES ? (size_t)e->size : INK_IMAGE_PROBE_BYTES;
    uint8_t *head = want ? osal_malloc(want, OSAL_MEM_EXTERNAL) : NULL;
    if (head && !ink_fs_open(e->path, INK_FS_READ, &f)) {
        if (ink_fs_read(f, head, want) == (int64_t)want) ink_image_probe(head, want, &p->info);
        if (ink_fs_close(f)) p->info.format = INK_IMAGE_UNKNOWN;
    }
    osal_free(head);
    if (p->info.format != INK_IMAGE_UNKNOWN && p->info.format != INK_IMAGE_JPEG && p->info.format != INK_IMAGE_PNG) p->seen = SEEN_FORMAT;
    else if (e->size > INK_IMAGE_MAX_INPUT || (uint64_t)p->info.width * p->info.height > INK_IMAGE_MAX_PIXELS) p->seen = SEEN_TOO_BIG;
}
static float fit(unsigned dw, unsigned dh, unsigned w, unsigned h) { return fminf((float)w / (float)dw, (float)h / (float)dh); }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static void clamp_view(view_t *v, unsigned dw, unsigned dh) {
    if (!dw || !dh || !v->w || !v->h) return;
    float s = fit(dw, dh, v->w, v->h) * (float)v->zoom, vw = (float)v->w / s, vh = (float)v->h / s;
    if (v->cx < 0) v->cx = dw / 2.f;
    if (v->cy < 0) v->cy = dh / 2.f;
    v->cx = vw >= dw ? dw / 2.f : clampf(v->cx, vw / 2, dw - vw / 2);
    v->cy = vh >= dh ? dh / 2.f : clampf(v->cy, vh / 2, dh - vh / 2);
}
/* The visible part only, scaled with two-by-two sampling, then dithered: never dither first and enlarge. */
static void render(load_t *p) {
    const ink_image_t *o = &M_ORIGINAL; view_t *v = &p->v;
    unsigned W = o->width, H = o->height, dw = v->rot & 1 ? H : W, dh = v->rot & 1 ? W : H;
    p->dw = dw; p->dh = dh;
    if (!o->pixels || !v->w || !v->h) return;
    clamp_view(v, dw, dh);
    ink_image_t out = {v->w, v->h, osal_malloc((size_t)v->w * v->h, OSAL_MEM_EXTERNAL)};
    if (!out.pixels) { p->err = INK_FS_ERR_IO; return; }
    float s = fit(dw, dh, v->w, v->h) * (float)v->zoom;
    for (unsigned y = 0; y < v->h; y++) {
        float dy = v->cy + ((float)y + .5f - v->h / 2.f) / s - .5f;
        for (unsigned x = 0; x < v->w; x++) {
            float dx = v->cx + ((float)x + .5f - v->w / 2.f) / s - .5f, u, w;
            uint8_t *dst = &out.pixels[(size_t)y * v->w + x];
            if (dx < -.5f || dy < -.5f || dx > dw - .5f || dy > dh - .5f) { *dst = 255; continue; }
            switch (v->rot) {
            case 1: u = dy; w = (float)H - 1 - dx; break;
            case 2: u = (float)W - 1 - dx; w = (float)H - 1 - dy; break;
            case 3: u = (float)W - 1 - dy; w = dx; break;
            default: u = dx; w = dy; break;
            }
            u = clampf(u, 0, (float)W - 1); w = clampf(w, 0, (float)H - 1);
            unsigned x0 = (unsigned)u, y0 = (unsigned)w, x1 = x0 + 1 < W ? x0 + 1 : x0, y1 = y0 + 1 < H ? y0 + 1 : y0;
            float fx = u - (float)x0, fy = w - (float)y0;
            const uint8_t *r0 = o->pixels + (size_t)y0 * W, *r1 = o->pixels + (size_t)y1 * W;
            *dst = (uint8_t)((r0[x0] * (1 - fx) + r0[x1] * fx) * (1 - fy) + (r1[x0] * (1 - fx) + r1[x1] * fx) * fy + .5f);
        }
        osal_cost_cycles((uint64_t)v->w * 48);
    }
    ink_image_dither(&out);
    p->im = out;
}
static void decode(load_t *p, const ink_fs_entry_t *e) {
    ink_image_free(&M_ORIGINAL);
    p->size = e->size; p->mtime = e->mtime; snprintf(p->path, sizeof p->path, "%s", e->path);
    photo_seen(p, e);
    if (p->seen) return;
    ink_fs_file_t *f = NULL;
    void *data = e->size > 0 ? osal_malloc((size_t)e->size, OSAL_MEM_EXTERNAL) : NULL;
    if (!data || ink_fs_open(e->path, INK_FS_READ, &f)) p->seen = SEEN_BROKEN;
    if (f) {
        if (ink_fs_read(f, data, (size_t)e->size) != e->size || ink_image_decode(data, (size_t)e->size, &M_ORIGINAL)) p->seen = SEEN_BROKEN;
        if (ink_fs_close(f)) p->seen = SEEN_BROKEN;
    }
    osal_free(data);
    if (!p->seen) render(p);
}
static void loaded(void *arg);
static void read_photos(void *arg);
static void read_again(void *arg) { /* timer task: the read belongs to the media executor */
    load_t *p = arg;
    if (ink_exec_post(INK_EXEC_MEDIA, &p->job, read_photos, p)) { p->err = INK_FS_ERR_BUSY; ink_exec_post(INK_EXEC_UI, &p->job, loaded, p); }
}
static void read_photos(void *arg) {
    load_t *p = arg; ink_fs_entry_t e;
    if (p->epoch != ink_member_epoch()) p->err = INK_FS_ERR_DENIED;
    else if (!p->id) {
        p->data = osal_calloc(1, sizeof *p->data, OSAL_MEM_EXTERNAL);
        if (!p->data) p->err = INK_FS_ERR_IO;
        else {
            scan(p->data, "/me", p->epoch, true);
            scan(p->data, LOCK_ALBUM, p->epoch, false);
            char family[40]; trash_root("/family", family, sizeof family);
            if (p->adult) scan(p->data, family, p->epoch, true);
            organize(p->data); p->err = p->data->err;
        }
    } else {
        p->revision = ink_fs_revision();
        /* 0 means the files are busy (an index batch, a write), not a version: the photo.ask check needs a real one */
        if (!p->revision && p->waits++ < 50) { ink_svc_at(&p->retry, osal_now_us() + 100000ull, read_again, p); return; }
        p->err = ink_fs_lookup(p->id, &e);
        if (!p->err) decode(p, &e);
    }
    ink_exec_post(INK_EXEC_UI, &p->job, loaded, p);
}
static void rendered(void *arg);
static void render_job(void *arg) { load_t *p = arg; render(p); ink_exec_post(INK_EXEC_UI, &p->job, rendered, p); }
static void release_job(void *arg) { ink_image_free(&M_ORIGINAL); osal_free(arg); }

/* ------------------------------------------------------------------ the list on screen */

static void filtered(void) {
    G.count = 0;
    for (size_t i = 0; G.data && i < G.data->count; i++) {
        const ink_gallery_item_t *it = &G.data->items[i];
        if ((G.filter == FILTER_ALBUM && it->album != G.album) || (G.filter == FILTER_MONTH && strcmp(it->month, G.month))) continue;
        G.ids[G.count++] = it->id;
    }
}
static void load(uint32_t id, uint32_t open_id) {
    load_t *p = osal_calloc(1, sizeof *p, OSAL_MEM_EXTERNAL);
    if (!p) { snprintf(G.message, sizeof G.message, "%s", TR("内存不足，请返回后重试")); return; }
    p->id = id; p->open_id = open_id; p->epoch = ink_member_epoch(); p->generation = ink_ui_generation(); p->seq = ++G.seq;
    p->adult = ink_member_current()->role != INK_ROLE_CHILD; p->v = G.v;
    if (ink_exec_post(INK_EXEC_MEDIA, &p->job, read_photos, p)) { osal_free(p); snprintf(G.message, sizeof G.message, "%s", TR("暂时读不了照片，请重试")); }
}
static const char *read_error(int err) {
    switch (err) {
    case INK_FS_ERR_NO_CARD: return TR("没插存储卡，插上后照片会自动出来");
    case INK_FS_ERR_USB: return TR("存储卡借给了电脑，拔线或在电脑上推出后自动回来");
    case INK_FS_ERR_INDEX_FULL: return TR("文件太多，有些照片没列出来");
    default: return TR("暂时读不了照片，请点重读照片");
    }
}
static bool organizing(void) {
    char s[24] = ""; ink_state_get_str(ink_state_find("file.index_state"), s, sizeof s);
    return strcmp(s, "ready") != 0;
}
static void bound_elsewhere(lv_event_t *e) { (void)e; } /* the shared empty state draws its action only with a callback */
static void empty_state(lv_obj_t *parent, const char *text, const char *action, const char *cap) { /* long sentences wrap */
    lv_obj_t *box = ink_ui_empty(parent, text, action, action ? bound_elsewhere : NULL, NULL);
    lv_obj_t *label = lv_obj_get_child(box, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP); lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_obj_update_layout(box);
    int h = lv_obj_get_height(label);
    if (action) {
        lv_obj_t *b = lv_obj_get_child(box, 1);
        lv_obj_set_y(b, h + ink_ui_px(INK_GAP)); ink_shell_bind(b, cap, NULL);
        h += ink_ui_px(INK_GAP + INK_CONTROL);
    }
    lv_obj_set_height(box, h); lv_obj_center(box);
}

static void thumb_flush(void) { /* what has come so far, drawn and refreshed as one */
    bool any = false;
    for (unsigned i = 0; i < PHOTO_SLOTS; i++) {
        if (!G.slots[i] || G.thumb_state[i] <= 0) continue;
        lv_obj_clean(G.slots[i]);
        if (G.thumb_state[i] == 1) ink_photo_image(G.slots[i], &G.thumbs[i]);
        else {
            ink_ui_icon(G.slots[i], "warn", INK_ICON_SMALL, INK_ICON_LINE, INK_GAP_S, INK_GAP_S, false);
            lv_obj_t *l = ink_ui_label(G.slots[i], TR("看不了预览"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_S, INK_GAP_S + INK_ICON_SMALL + INK_GAP_XS, 0);
            lv_obj_set_width(l, lv_obj_get_width(G.slots[i]) - 2 * ink_ui_px(INK_GAP_S));
        }
        ink_image_free(&G.thumbs[i]); G.thumb_state[i] = -1; any = true;
    }
    if (any) ink_ui_hint_obj(lv_obj_get_parent(G.slots[0]), INK_REFRESH_UI);
}
static void thumb_arrived(void) { /* the whole page at once; after the deadline, the rest when it is all there */
    if (++G.arrived < G.expected) return;
    ink_svc_cancel(&G.batch_at); thumb_flush();
    if (G.fresh) ink_probe_count("gallery.page_ready"); /* for tests: this page has nothing more to draw */
}
typedef struct { uint32_t epoch, generation, id, batch; unsigned slot; } thumb_t;
static void thumb_ready(void *ctx, const ink_receipt_t *rc) {
    thumb_t *t = ctx;
    if (t->epoch == ink_member_epoch() && t->generation == ink_ui_generation() && t->batch == G.batch && G.list && G.slots[t->slot] && !G.thumb_state[t->slot]) {
        ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t w = 0, h = 0; G.thumb_state[t->slot] = 2;
        if (j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result))) {
            ink_json_int(j, ink_json_find(j, 0, "width"), &w); ink_json_int(j, ink_json_find(j, 0, "height"), &h);
            int tok = ink_json_find(j, 0, "gray4");
            if (tok >= 0 && w > 0 && h > 0 && w <= INK_THUMB_EDGE && h <= INK_THUMB_EDGE && j->tok[tok].end - j->tok[tok].start == w * h) {
                ink_image_t im = {(unsigned)w, (unsigned)h, osal_malloc((size_t)w * h, OSAL_MEM_EXTERNAL)};
                if (im.pixels) {
                    const char *s = rc->result + j->tok[tok].start;
                    for (int64_t i = 0; i < w * h; i++) im.pixels[i] = (uint8_t)((s[i] <= '9' ? s[i] - '0' : s[i] - 'a' + 10) * 17);
                    G.thumbs[t->slot] = im; G.thumb_state[t->slot] = 1;
                }
            }
        }
        osal_free(j);
        thumb_arrived();
    }
    osal_free(t);
}
static void batch_late(void *arg) { if ((uint32_t)(uintptr_t)arg == G.batch && G.list && G.slots[0]) thumb_flush(); }
static void batch_due(void *ctx) { ink_exec_post(INK_EXEC_UI, &G.batch_job, batch_late, ctx); } /* timer task */
static void bind_photo(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % PHOTO_SLOTS; G.slots[at] = slot; lv_obj_clean(slot);
    lv_obj_set_style_outline_width(slot, ink_ui_px(INK_STROKE), LV_STATE_PRESSED);
    lv_obj_set_style_outline_color(slot, ink_ui_ink(), LV_STATE_PRESSED);
    const ink_gallery_item_t *it = item_of(G.ids[index]);
    snprintf(G.objects[at], sizeof G.objects[at], "file:%u", (unsigned)G.ids[index]);
    snprintf(G.args[at], sizeof G.args[at], "{\"id\":%u}", (unsigned)G.ids[index]);
    snprintf(G.labels[at], sizeof G.labels[at], "%s", it ? it->name : TR("照片"));
    day_text(it ? it->modified : -1, G.values[at], sizeof G.values[at]);
    *field = (ink_ui_field_spec_t){.id = G.objects[at], .role = "image", .label = G.labels[at], .value = G.values[at], .object = {G.objects[at], 1}};
    *action = (ink_ui_action_spec_t){.id = G.objects[at], .label = TR("查看照片"), .capability = "photo.open", .args_json = G.args[at], .object = field->object, .enabled = true};
    if (!at) { /* a new page: a new batch, only for the cells that show */
        G.batch++; G.arrived = 0; memset(G.thumb_state, 0, sizeof G.thumb_state);
        for (unsigned i = 0; i < PHOTO_SLOTS; i++) ink_image_free(&G.thumbs[i]);
        G.expected = (unsigned)(G.count - index < PHOTO_SLOTS ? G.count - index : PHOTO_SLOTS);
        ink_svc_at(&G.batch_at, osal_now_us() + GALLERY_BATCH_MS * 1000ull, batch_due, (void *)(uintptr_t)G.batch);
    }
    thumb_t *t = osal_malloc(sizeof *t, OSAL_MEM_EXTERNAL);
    if (t) *t = (thumb_t){G.epoch, ink_ui_generation(), G.ids[index], G.batch, at};
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .args_json = G.args[at], .reply_exec = INK_EXEC_UI, .done = thumb_ready, .ctx = t};
    if (!t || ink_cap_call("file.thumb", NULL, &o)) { osal_free(t); G.thumb_state[at] = 2; thumb_arrived(); }
}
static size_t album_rows(void) { return G.data ? G.data->nalbums + (G.data->ntrash ? 1 : 0) : 0; }
static void bind_row(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % LIST_SLOTS; lv_obj_clean(slot);
    const char *icon = "calendar", *cap = "gallery.find";
    if (G.view == VIEW_ALBUMS && index == G.data->nalbums) {
        icon = "trash"; cap = "gallery.show";
        snprintf(G.labels[at], sizeof G.labels[at], "%s", TR("最近删除"));
        snprintf(G.values[at], sizeof G.values[at], TR("%zu 张"), G.data->ntrash);
        snprintf(G.args[at], sizeof G.args[at], "{\"view\":\"trash\"}");
    } else if (G.view == VIEW_ALBUMS) {
        const album_t *a = &G.data->albums[index]; char day[48]; day_text(a->newest, day, sizeof day); icon = "folder";
        snprintf(G.labels[at], sizeof G.labels[at], "%s", album_name(a->path));
        snprintf(G.values[at], sizeof G.values[at], TR("%u 张 · 最新 %s"), (unsigned)a->count, day);
        ink_jw_t w; ink_jw_init(&w, G.args[at], sizeof G.args[at]); ink_jw_raw(&w, "{\"album\":"); ink_jw_str(&w, a->path); ink_jw_raw(&w, "}");
    } else {
        const month_t *m = &G.data->months[index]; month_label(m->key, G.labels[at], sizeof G.labels[at]);
        snprintf(G.values[at], sizeof G.values[at], TR("%u 张"), (unsigned)m->count);
        snprintf(G.args[at], sizeof G.args[at], "{\"month\":\"%s\"}", m->key);
    }
    snprintf(G.objects[at], sizeof G.objects[at], "gallery:row%zu", index);
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    if (index % LIST_SLOTS) ink_ui_group_divider(slot, 0, w);
    lv_obj_t *row = ink_ui_list_row(slot, icon, G.labels[at], G.values[at], 0, 0, w, true, NULL, NULL);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    *field = (ink_ui_field_spec_t){.id = G.objects[at], .role = "text", .label = G.labels[at], .value = G.values[at], .object = {G.objects[at], 1}};
    *action = (ink_ui_action_spec_t){.id = G.objects[at], .label = G.labels[at], .capability = cap, .args_json = G.args[at], .object = field->object, .enabled = true};
}
static const ink_ui_action_spec_t *turns(void) { /* copied by the list */
    static ink_ui_action_spec_t a[2] INK_PSRAM_BSS;
    a[0] = (ink_ui_action_spec_t){.id = "prev", .label = TR("上一页"), .capability = "gallery.prev"};
    a[1] = (ink_ui_action_spec_t){.id = "next", .label = TR("下一页"), .capability = "gallery.next"};
    return a;
}
static void line_text(void) { /* which month this page is, or what the filter shows */
    G.line[0] = 0;
    if (G.filter && G.view == VIEW_ALL) {
        char what[96];
        if (G.filter == FILTER_ALBUM) snprintf(what, sizeof what, TR("相簿：%s"), album_name(G.data->albums[G.album].path)); else month_label(G.month, what, sizeof what);
        snprintf(G.line, sizeof G.line, TR("%s · %zu 张"), what, G.count);
    } else if (G.count && G.view == VIEW_ALL) {
        size_t first = G.position.page * PHOTO_SLOTS, last = first + PHOTO_SLOTS < G.count ? first + PHOTO_SLOTS - 1 : G.count - 1;
        const ink_gallery_item_t *a = item_of(G.ids[first < G.count ? first : 0]), *b = item_of(G.ids[last]);
        char la[32], lb[32]; month_label(a ? a->month : "", la, sizeof la); month_label(b ? b->month : "", lb, sizeof lb);
        int ya = 0, ma = 0, yb = 0, mb = 0;
        if (!strcmp(la, lb)) snprintf(G.line, sizeof G.line, "%s", la);
        else if (a && b && sscanf(a->month, "%d-%d", &ya, &ma) == 2 && sscanf(b->month, "%d-%d", &yb, &mb) == 2 && ya == yb) snprintf(G.line, sizeof G.line, TR("%d月—%d月"), ma, mb);
        else snprintf(G.line, sizeof G.line, "%s—%s", la, lb);
    }
}
static void draw_content(void) {
    if (!G.content) return;
    lv_obj_clean(G.content); G.list = NULL; G.status = G.month_line = NULL; G.waiting = false; memset(G.slots, 0, sizeof G.slots);
    G.batch++; ink_svc_cancel(&G.batch_at);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, h = INK_CANVAS_H - INK_MARGIN - (INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP_S + INK_SEGMENT + INK_GAP), y = 0;
    bool empty = !G.data || (!G.data->count && !G.data->ntrash);
    if (!G.data) { G.status = ink_ui_notice(G.content, TR("正在读取照片"), 0); return; }
    if (empty && G.data->err) { empty_state(G.content, read_error(G.data->err), TR("重读照片"), "gallery.reload"); return; }
    if (empty && organizing()) { G.waiting = true; empty_state(G.content, TR("正在整理照片，好了会自动出来"), NULL, NULL); return; }
    if (G.data->err) { G.status = ink_ui_notice(G.content, read_error(G.data->err), 0); y += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S; }
    if (G.view == VIEW_ALL) {
        line_text();
        if (G.filter) {
            int bw = LV_MAX(INK_CONTROL, ink_ui_text_width(TR("清除"), INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP);
            lv_obj_t *l = ink_ui_line(G.content, G.line, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y + (INK_CONTROL - ink_ui_line_height(INK_FONT_NOTE)) / 2, w - bw);
            lv_obj_set_style_text_color(l, ink_ui_secondary(), 0);
            lv_obj_t *b = ink_ui_text_button(G.content, TR("清除"), INK_CANVAS_W - INK_MARGIN - bw, y, bw, NULL, NULL);
            ink_shell_bind(b, "gallery.find", "{}"); y += INK_CONTROL + INK_GAP_S;
        } else if (G.count) {
            lv_obj_t *l = G.month_line = ink_ui_line(G.content, G.line, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
            lv_obj_set_style_text_color(l, ink_ui_secondary(), 0); y += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S;
        }
        int cell = (w - (PHOTO_COLUMNS - 1) * INK_GAP_M) / PHOTO_COLUMNS, room = (h - y - INK_GAP - INK_CONTROL - 2 * INK_GAP_M) / 3;
        lv_obj_t *grid = ink_ui_box(G.content, 0, y, INK_CANVAS_W, h - y, 0, INK_SURFACE_WHITE, false);
        const ink_ui_page_spec_t page = {.id = "Gallery", .title = TR("相册"), .object = {"gallery:photos", 1}, .actions = turns(), .action_count = 2};
        G.list = ink_ui_list_create(grid, &page, &G.position, PHOTO_SLOTS, PHOTO_COLUMNS, cell < room ? cell : room,
                                    G.filter ? TR("这个相簿里没有照片了") : TR("还没有照片，点右上角拍一张"), bind_photo, NULL);
        if (G.list) ink_ui_list_refresh(G.list, G.count);
        return;
    }
    int row = ink_ui_row_double(), capacity = (h - y - INK_GAP - INK_CONTROL) / (row + INK_GAP_M);
    capacity = capacity < 1 ? 1 : capacity > LIST_SLOTS ? LIST_SLOTS : capacity;
    lv_obj_t *list = ink_ui_box(G.content, 0, y, INK_CANVAS_W, h - y, 0, INK_SURFACE_WHITE, false);
    const ink_ui_page_spec_t page = {.id = "Gallery", .title = TR("相册"), .object = {"gallery:rows", 1}, .actions = turns(), .action_count = 2};
    G.list = ink_ui_list_create(list, &page, &G.rows, (unsigned)capacity, 1, row, TR("还没有照片，点右上角拍一张"), bind_row, NULL);
    if (G.list) ink_ui_list_refresh(G.list, G.view == VIEW_ALBUMS ? album_rows() : G.data->nmonths);
}
static void build_content(void) {
    G.expected = 0; draw_content();
    if (G.fresh && !G.expected) ink_probe_count("gallery.page_ready"); /* else when the page's thumbnails are in */
}
static bool same_list(const photos_t *a, const photos_t *b) {
    if (!a || !b || a->count != b->count || a->ntrash != b->ntrash || a->err != b->err) return false;
    for (size_t i = 0; i < a->count; i++) if (a->items[i].id != b->items[i].id || strcmp(a->items[i].path, b->items[i].path)) return false;
    for (size_t i = 0; i < a->ntrash; i++) if (strcmp(a->trash[i].path, b->trash[i].path)) return false;
    return true;
}

/* ------------------------------------------------------------------ one photo */

static void photo_open_id(uint32_t id);
static bool zoomed(void) { return G.v.zoom > 1; }
static int tool_height(void) { return LV_MAX(INK_CONTROL, INK_ICON_SMALL + 3 * INK_GAP_XS + ink_ui_line_height(INK_FONT_LIST)); }
static int tools(tool_t *t) {
    static const char *const TOOL_ICONS[] = {"back", "next", "plus", "minus", "rotate", "info", "trash"};
    static const char *const TOOL_CAPS[] = {"photo.prev", "photo.next", "photo.zoom", "photo.zoom", "photo.rotate", "photo.info", "photo.trash"};
    static const char *const TOOL_ARGS[] = {NULL, NULL, "{\"step\":\"in\"}", "{\"step\":\"out\"}", "{\"dir\":\"right\"}", NULL, NULL};
    const char *labels[] = {TR("上一张"), TR("下一张"), TR("放大"), TR("缩小"), TR("旋转"), TR("信息"), TR("删除")};
    for (int i = 0; i < 7; i++) t[i] = (tool_t){TOOL_ICONS[i], labels[i], TOOL_CAPS[i], TOOL_ARGS[i]};
    return 7;
}
static int tool_width(const tool_t *t) { return LV_MAX(INK_CONTROL, ink_ui_text_width(t->label, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP_S); }
static int tool_rows(const tool_t *t, int n, int *row) { /* left to right, a new row when one does not fit */
    int w = INK_CANVAS_W - 2 * INK_MARGIN, x = 0, r = 0;
    for (int i = 0; i < n; i++) {
        int tw = tool_width(&t[i]);
        if (x && x + tw > w) { r++; x = 0; }
        row[i] = r; x += tw + INK_GAP_S;
    }
    return r + 1;
}
/* Where things go: status line (when there is something to say), photo, tools (and the pan row when zoomed). */
typedef struct { int status_h, box_y, box_h, bars_y, rows; } layout_t;
static int layout_key(void) { return (zoomed() ? 1 : 0) | (G.message[0] ? 2 : 0); }
static layout_t photo_layout(void) {
    tool_t t[8]; int row[8], n = tools(t); layout_t l = {0};
    l.rows = tool_rows(t, n, row);
    int body = INK_CANVAS_H - (INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP_S + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S);
    int bars = l.rows * tool_height() + (l.rows - 1) * INK_GAP_S + (zoomed() ? INK_GAP_S + INK_CONTROL : 0);
    l.status_h = G.message[0] ? INK_CONTROL + INK_GAP_S : 0;
    l.box_y = l.status_h; l.bars_y = body - INK_MARGIN - bars; l.box_h = l.bars_y - INK_GAP_M - l.box_y;
    return l;
}
static void photo_target(view_t *v) {
    layout_t l = photo_layout();
    v->w = (unsigned)ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN); v->h = (unsigned)LV_MAX(1, ink_ui_px(l.box_h));
}
static void photo_box_event(lv_event_t *e);
static void show_view(void) {
    if (!G.box) return;
    lv_obj_clean(G.box); G.image = NULL; G.notice[0] = 0;
    if (G.seen) { /* why it cannot be shown, in one sentence (also for the AI) */
        char *text = G.notice; size_t cap = sizeof G.notice;
        if (G.seen == SEEN_TOO_BIG) snprintf(text, cap, TR("这张照片太大（%u×%u），设备只能看 100 万像素以内的"), G.info.width, G.info.height);
        else if (G.seen == SEEN_FORMAT) snprintf(text, cap, TR("这种格式（%s）还看不了"), G.info.format == INK_IMAGE_WEBP ? "WebP" : G.info.format == INK_IMAGE_GIF ? "GIF" : "BMP");
        else snprintf(text, cap, "%s", TR("这张照片读不出来，文件可能坏了"));
        lv_obj_t *l = ink_ui_label(G.box, text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, 0, INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0); lv_obj_set_style_text_color(l, ink_ui_secondary(), 0); lv_obj_center(l);
        return;
    }
    if (!G.shown.pixels || G.view_photo != G.photo) return;
    G.dsc = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_L8, .w = G.shown.width, .h = G.shown.height,
        .stride = G.shown.width}, .data_size = G.shown.width * G.shown.height, .data = G.shown.pixels};
    G.image = lv_image_create(G.box); lv_image_set_src(G.image, &G.dsc); lv_obj_set_pos(G.image, 0, 0);
    lv_obj_remove_flag(G.image, LV_OBJ_FLAG_CLICKABLE);
}
static void tool_button(lv_obj_t *parent, const tool_t *t, int x, int y, int w) {
    lv_obj_t *b = ink_ui_text_button(parent, t->label, x, y, w, NULL, NULL);
    lv_obj_set_height(b, ink_ui_px(tool_height()));
    ink_ui_icon(b, t->icon, INK_ICON_SMALL, INK_ICON_LINE, (w - INK_ICON_SMALL) / 2 - INK_GAP_XS, INK_GAP_XS, false);
    lv_obj_align(lv_obj_get_child(b, 0), LV_ALIGN_BOTTOM_MID, 0, -ink_ui_px(INK_GAP_XS));
    ink_shell_bind(b, t->cap, t->args);
}
static void build_body(void) {
    if (!G.body) return;
    lv_obj_clean(G.body); G.box = G.image = G.status = NULL; G.layout = layout_key();
    layout_t l = photo_layout(); int w = INK_CANVAS_W - 2 * INK_MARGIN;
    if (G.message[0]) {
        const char *act = G.undo_args[0] ? TR("撤销") : G.forever_args[0] ? TR("永久删除") : NULL;
        int bw = act ? LV_MAX(INK_CONTROL, ink_ui_text_width(act, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP) : 0;
        G.status = ink_ui_label(G.body, G.message, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, 0, w - bw - INK_GAP_S);
        lv_obj_set_style_text_color(G.status, ink_ui_secondary(), 0); lv_obj_update_layout(G.status);
        int row = ink_ui_px(INK_CONTROL), th = lv_obj_get_height(G.status); /* centred beside its button, cut at the row */
        if (th > row) { lv_obj_set_height(G.status, row); lv_label_set_long_mode(G.status, LV_LABEL_LONG_DOT); th = row; }
        lv_obj_set_y(G.status, (row - th) / 2);
        if (act) {
            lv_obj_t *b = ink_ui_text_button(G.body, act, INK_CANVAS_W - INK_MARGIN - bw, 0, bw, NULL, NULL);
            ink_shell_bind(b, G.undo_args[0] ? "photo.restore" : "photo.delete_forever", G.undo_args[0] ? G.undo_args : G.forever_args);
        }
    }
    G.box = ink_ui_box(G.body, INK_MARGIN, l.box_y, w, l.box_h, 0, INK_SURFACE_WHITE, false);
    lv_obj_add_flag(G.box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(G.box, photo_box_event, LV_EVENT_ALL, NULL); ink_ui_capture_touch(G.box);
    tool_t t[8]; int row[8], n = tools(t); tool_rows(t, n, row);
    for (int r = 0, i = 0; r < l.rows; r++) {
        int first = i, used = 0, y = l.bars_y + r * (tool_height() + INK_GAP_S);
        while (i < n && row[i] == r) used += tool_width(&t[i++]);
        int extra = (w - used - (i - first - 1) * INK_GAP_S) / (i - first), x = INK_MARGIN;
        for (int k = first; k < i; k++) { int tw = tool_width(&t[k]) + extra; tool_button(G.body, &t[k], x, y, tw); x += tw + INK_GAP_S; }
    }
    if (zoomed()) {
        static const char *const PAN_DIRS[] = {"{\"dir\":\"left\"}", "{\"dir\":\"right\"}", "{\"dir\":\"up\"}", "{\"dir\":\"down\"}"};
        const char *names[] = {TR("左移"), TR("右移"), TR("上移"), TR("下移")};
        int y = l.bars_y + l.rows * (tool_height() + INK_GAP_S), bw = (w - 3 * INK_GAP_S) / 4;
        for (int k = 0; k < 4; k++) ink_shell_bind(ink_ui_text_button(G.body, names[k], INK_MARGIN + k * (bw + INK_GAP_S), y, bw, NULL, NULL), "photo.pan", PAN_DIRS[k]);
    }
    show_view();
}
static void caption(void) {
    const ink_gallery_item_t *it = item_of(G.photo); size_t at = index_of(G.photo);
    const char *name = base_name(G.path); char day[48] = "";
    if (it || G.mtime > 0) day_text(it ? it->modified : G.mtime / 1000000000, day, sizeof day);
    if (at < G.count) snprintf(G.caption, sizeof G.caption, TR("%s · 第%zu/%zu张 · %s"), name, at + 1, G.count, day);
    else snprintf(G.caption, sizeof G.caption, "%s", name);
    if (G.name) lv_label_set_text(G.name, G.caption);
}
static void photo_changed_view(void) { /* one refresh: the body when its layout changes, else the photo only */
    if (!G.body) return;
    if (G.layout != layout_key()) { build_body(); ink_ui_hint_obj(G.body, INK_REFRESH_UI); }
    else { show_view(); if (G.box) ink_ui_hint_obj(G.box, INK_REFRESH_UI); }
    ink_ui_content_changed(); ink_probe_count("gallery.photo_drawn"); /* for tests */
}
static void rendered(void *arg) {
    load_t *p = arg;
    if (p->epoch == ink_member_epoch() && p->seq == G.seq && p->id == G.photo && p->im.pixels) {
        ink_image_free(&G.shown); G.shown = p->im; p->im = (ink_image_t){0}; G.view_photo = p->id;
        G.v.cx = p->v.cx; G.v.cy = p->v.cy; G.dw = p->dw; G.dh = p->dh;
        if (ink_ui_page_current() && !strcmp(ink_ui_page_name(), "Photo")) photo_changed_view();
    }
    ink_image_free(&p->im); osal_free(p);
}
static void request_render(void) { /* the media executor runs one at a time; the newest request wins */
    load_t *p = osal_calloc(1, sizeof *p, OSAL_MEM_EXTERNAL);
    if (!p) return;
    photo_target(&G.v); clamp_view(&G.v, G.dw, G.dh);
    p->id = G.photo; p->epoch = ink_member_epoch(); p->seq = ++G.seq; p->v = G.v;
    if (ink_exec_post(INK_EXEC_MEDIA, &p->job, render_job, p)) osal_free(p);
}
static void clear_status(void) { G.message[0] = G.undo_args[0] = G.forever_args[0] = 0; }
static void zoom_to(int zoom, float cx, float cy) {
    clear_status(); G.v.zoom = zoom; G.v.cx = cx; G.v.cy = cy; request_render();
}
static bool pan(float fx, float fy) { /* by a share of the visible part; false when it cannot move */
    if (!zoomed() || !G.dw) return false;
    view_t v = G.v; photo_target(&v);
    float s = fit(G.dw, G.dh, v.w, v.h) * (float)v.zoom;
    v.cx += fx * v.w / s; v.cy += fy * v.h / s; clamp_view(&v, G.dw, G.dh);
    if (fabsf(v.cx - G.v.cx) < .5f && fabsf(v.cy - G.v.cy) < .5f) return false;
    clear_status(); G.v.cx = v.cx; G.v.cy = v.cy; request_render(); return true;
}
static void photo_box_event(lv_event_t *e) { /* single taps do nothing; double taps zoom; drags pan or turn */
    lv_event_code_t code = lv_event_get_code(e); lv_point_t p; lv_indev_t *in = lv_indev_active();
    if (!in || (code != LV_EVENT_PRESSED && code != LV_EVENT_RELEASED)) return;
    lv_indev_get_point(in, &p);
    if (code == LV_EVENT_PRESSED) { G.press_at = p; return; }
    int dx = p.x - G.press_at.x, dy = p.y - G.press_at.y, near = ink_ui_px(INK_GAP_L);
    if (abs(dx) > near || abs(dy) > near) {
        G.tap_us = 0;
        if (zoomed() && G.dw) {
            float s = fit(G.dw, G.dh, G.v.w, G.v.h) * (float)G.v.zoom;
            view_t v = G.v; v.cx -= dx / s; v.cy -= dy / s; clamp_view(&v, G.dw, G.dh);
            clear_status(); G.v = v; request_render();
        } else if (abs(dx) >= ink_ui_px(INK_BLOCK_L) && abs(dx) > abs(dy)) {
            ink_receipt_t rc; ink_cap_invoke(dx < 0 ? "photo.next" : "photo.prev", NULL, INK_SRC_TOUCH, 0, &rc);
        }
        return;
    }
    uint64_t now = osal_now_us();
    if (G.tap_us && now - G.tap_us < GALLERY_DOUBLE_TAP_MS * 1000ull && abs(p.x - G.tap_at.x) < 2 * near && abs(p.y - G.tap_at.y) < 2 * near) {
        G.tap_us = 0;
        if (zoomed() || !G.dw || G.seen) { if (zoomed()) zoom_to(1, -1, -1); return; }
        lv_area_t a; lv_obj_get_coords(G.box, &a);
        float s = fit(G.dw, G.dh, G.v.w, G.v.h);
        zoom_to(2, G.v.cx + (p.x - a.x1 - G.v.w / 2.f) / s, G.v.cy + (p.y - a.y1 - G.v.h / 2.f) / s);
        return;
    }
    G.tap_us = now; G.tap_at = p; /* the first tap: nothing changes, nothing is drawn */
}
static void photo_loaded(load_t *p) {
    if (p->id != G.photo || !G.body) return;
    G.revision = p->revision; G.seen = p->err ? SEEN_BROKEN : p->seen; G.info = p->info; G.size = p->size; G.mtime = p->mtime;
    snprintf(G.path, sizeof G.path, "%s", p->path); G.dw = p->dw; G.dh = p->dh;
    if (p->im.pixels) { ink_image_free(&G.shown); G.shown = p->im; p->im = (ink_image_t){0}; G.view_photo = p->id; G.v.cx = p->v.cx; G.v.cy = p->v.cy; }
    caption();
    if (G.layout != layout_key()) build_body(); else show_view();
    ink_ui_hint_obj(G.body, INK_REFRESH_UI); ink_ui_hint_obj(G.name, INK_REFRESH_UI); ink_ui_content_changed(); ink_probe_count("gallery.photo_drawn");
}
static void photo_open_id(uint32_t id) { /* the next or previous one: no blank in between, one refresh when it is ready */
    G.photo = G.last_photo = id; G.v = (view_t){.zoom = 1, .cx = -1, .cy = -1}; G.dw = G.dh = 0; G.seen = SEEN_OK;
    photo_target(&G.v); load(id, 0);
}

static void place_last(void) { /* back from a photo: its page, its cell outlined until the next step */
    size_t at = index_of(G.last_photo);
    if (at < G.count) { G.position.page = at / PHOTO_SLOTS; snprintf(G.position.selected, sizeof G.position.selected, "file:%u", (unsigned)G.last_photo); G.last_photo = 0; }
}
static void loaded(void *arg) {
    load_t *p = arg;
    if (p->epoch == ink_member_epoch() && p->generation == ink_ui_generation() && ink_ui_page_current()) {
        if (!p->id && p->data) {
            bool same = same_list(G.data, p->data), first = !G.data;
            osal_free(G.data); G.data = p->data; p->data = NULL;
            if (G.filter == FILTER_ALBUM && G.album >= G.data->nalbums) G.filter = FILTER_NONE;
            filtered();
            const char *page = ink_ui_page_name(); G.fresh = true;
            if (!strcmp(page, "Gallery") && (first || !same || (G.waiting && !organizing()))) {
                place_last(); build_content(); if (G.content) ink_ui_hint_obj(G.content, INK_REFRESH_UI);
            } else if (!strcmp(page, "Gallery") && G.arrived >= G.expected) ink_probe_count("gallery.page_ready");
            else if (!strcmp(page, "GalleryTrash") && !same) ink_ui_redraw();
            else if (!strcmp(page, "Photo")) { if (p->open_id) photo_open_id(p->open_id); else caption(); }
        } else if (p->id) photo_loaded(p);
        ink_ui_content_changed();
    }
    ink_image_free(&p->im); osal_free(p->data); osal_free(p);
}

/* ------------------------------------------------------------------ pages */

/* A text button at the right of the title; the short wording when the long one would run into the title. */
static lv_obj_t *top_button(lv_obj_t *root, const char *title, const char *label, const char *shorter, const char *cap, const char *args) {
    int room = INK_CANVAS_W - INK_MARGIN - (INK_MARGIN + INK_CONTROL + INK_GAP + ink_ui_text_width(title, INK_FONT_TITLE, INK_WEIGHT_BOLD) + INK_GAP);
    if (ink_ui_text_width(label, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP > room) label = shorter;
    int w = LV_MAX(INK_CONTROL, ink_ui_text_width(label, INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP);
    lv_obj_t *b = ink_ui_text_button(root, label, INK_CANVAS_W - INK_MARGIN - w, INK_STATUS_BAR + INK_GAP_S, w, NULL, NULL);
    ink_shell_bind(b, cap, args); return b;
}
static void segment_job(void *arg) { /* after the tap's own event: the segments it pressed are drawn anew */
    static const char *const VIEW_ARGS[] = {"{\"view\":\"all\"}", "{\"view\":\"albums\"}", "{\"view\":\"months\"}"};
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .args_json = VIEW_ARGS[(uintptr_t)arg], .reply_exec = INK_EXEC_UI, .member = ink_member_current()->id};
    ink_cap_call("gallery.show", NULL, &o);
}
static void segment_clicked(lv_event_t *e) {
    static ink_exec_job_t job INK_PSRAM_BSS;
    int at = (int)lv_obj_get_index(lv_event_get_target_obj(e));
    if (at >= 0 && at < 3 && at != G.view) ink_exec_post(INK_EXEC_UI, &job, segment_job, (void *)(uintptr_t)at);
}
static void purge_start(void);
static void draw_segments(void) { /* in their own box: a new row redraws that box, not the whole screen */
    const char *labels[] = {TR("全部"), TR("相簿"), TR("日期")};
    lv_obj_clean(G.segments);
    ink_ui_segments(G.segments, labels, 3, G.view, INK_MARGIN, 0, INK_CANVAS_W - 2 * INK_MARGIN, segment_clicked, NULL);
}
static void gallery_create(lv_obj_t *root, void *arg) {
    (void)arg;
    if (G.epoch != ink_member_epoch()) {
        memset(&G.position, 0, sizeof G.position); memset(&G.rows, 0, sizeof G.rows);
        G.epoch = ink_member_epoch(); G.count = 0; G.view = VIEW_ALL; G.filter = FILTER_NONE; G.last_photo = 0;
        osal_free(G.data); G.data = NULL;
    }
    ink_image_free(&G.shown); G.view_photo = 0;
    void *drop = osal_malloc(sizeof(ink_exec_job_t), OSAL_MEM_EXTERNAL);
    if (drop && ink_exec_post(INK_EXEC_MEDIA, drop, release_job, drop)) osal_free(drop);
    ink_shell_statusbar(root, true); ink_shell_topbar(root, TR("相册"));
    top_button(root, TR("相册"), TR("拍照"), TR("拍照"), "app.open", "{\"id\":\"camera\"}");
    G.segments = ink_ui_box(root, 0, INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP_S, INK_CANVAS_W, INK_SEGMENT, 0, INK_SURFACE_WHITE, false); draw_segments();
    int y = INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP_S + INK_SEGMENT + INK_GAP;
    G.content = ink_ui_box(root, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false);
    G.message[0] = 0; G.fresh = false; filtered(); place_last(); build_content(); load(0, 0); purge_start();
}
static void view_changed(void) { /* the segments and what is under them, nothing else */
    if (!G.content) { ink_ui_redraw(); return; }
    draw_segments(); build_content();
    ink_ui_hint_obj(G.segments, INK_REFRESH_UI); ink_ui_hint_obj(G.content, INK_REFRESH_UI); ink_ui_content_changed();
}
static void photo_create(lv_obj_t *root, void *arg) {
    uint32_t id = (uint32_t)(uintptr_t)arg;
    ink_shell_statusbar(root, true); ink_shell_topbar(root, TR("照片"));
    top_button(root, TR("照片"), TR("问 AI 这张图"), TR("问 AI"), "photo.ask", NULL);
    G.name = ink_ui_line(root, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_color(G.name, ink_ui_secondary(), 0);
    int y = INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP_S + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S;
    G.body = ink_ui_box(root, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false);
    if (id != G.opened || !G.view_photo) { /* a new photo; back from its details (the page keeps the id it opened with) it stays as it was */
        G.path[0] = G.caption[0] = 0; clear_status(); G.photo = G.opened = id; G.last_photo = id; G.v = (view_t){.zoom = 1, .cx = -1, .cy = -1};
        G.dw = G.dh = 0; G.seen = SEEN_OK; ink_image_free(&G.shown); G.view_photo = 0; G.info = (ink_image_info_t){0};
        build_body(); photo_target(&G.v); load(id, 0);
        lv_label_set_text(G.name, TR("正在读取照片"));
    } else { build_body(); caption(); }
}
static void gallery_destroy(void) {
    G.list = NULL; G.content = G.status = NULL; memset(G.slots, 0, sizeof G.slots); ink_svc_cancel(&G.batch_at); G.batch++;
    for (unsigned i = 0; i < PHOTO_SLOTS; i++) ink_image_free(&G.thumbs[i]);
}
static void photo_destroy(void) { G.box = G.status = G.name = G.body = G.image = NULL; G.tap_us = 0; }

static void info_rows(void) {
    const ink_gallery_item_t *it = item_of(G.photo); size_t at = index_of(G.photo), n = 0;
    char dir[INK_FS_PATH_MAX]; snprintf(dir, sizeof dir, "%.*s", (int)(base_name(G.path) - G.path - (G.path[0] ? 1 : 0)), G.path);
    G.info_labels[n] = TR("名字"); snprintf(G.info_values[n++], 96, "%.63s", G.path[0] ? base_name(G.path) : TR("不详"));
    G.info_labels[n] = TR("相簿"); snprintf(G.info_values[n++], 96, "%s", dir[0] ? album_name(dir) : TR("不详"));
    ink_tm_t t = {0}; int64_t when = G.mtime > 0 ? G.mtime / 1000000000 : -1;
    if (G.info.taken[0] && sscanf(G.info.taken, "%d:%d:%d %d:%d:%d", &t.year, &t.month, &t.day, &t.hour, &t.min, &t.sec) == 6) {
        char clock[16]; ink_gallery_time_label(ink_time_from_local(&t), clock, sizeof clock);
        G.info_labels[n] = TR("拍摄时间"); snprintf(G.info_values[n++], 96, TR("%d年%d月%d日 %s"), t.year, t.month, t.day, clock);
    } else if (dated(when)) {
        char clock[16]; ink_gallery_time_label(when, clock, sizeof clock); ink_time_to_local(when, &t);
        G.info_labels[n] = TR("拍摄时间"); snprintf(G.info_values[n++], 96, TR("%d年%d月%d日 %s（文件时间）"), t.year, t.month, t.day, clock);
    } else { G.info_labels[n] = TR("拍摄时间"); snprintf(G.info_values[n++], 96, "%s", TR("不详")); }
    if (G.info.width && G.info.height) { G.info_labels[n] = TR("尺寸"); snprintf(G.info_values[n++], 96, "%u×%u", G.info.width, G.info.height); }
    G.info_labels[n] = TR("大小");
    if (G.size > 0) ink_gallery_size_label(G.size, G.info_values[n++], 96); else snprintf(G.info_values[n++], 96, "%s", TR("不详"));
    static const char *const IMAGE_FORMATS[] = {NULL, "JPEG", "PNG", "GIF", "BMP", "WebP"};
    const char *ext = strrchr(G.path, '.'), *format = IMAGE_FORMATS[G.info.format];
    if (!format && ink_gallery_is_jpg(G.path)) format = "JPEG";
    G.info_labels[n] = TR("格式"); snprintf(G.info_values[n++], 96, "%s", format ? format : ext ? ext + 1 : TR("不详"));
    if (at < G.count) { G.info_labels[n] = TR("第几张"); snprintf(G.info_values[n++], 96, TR("第%zu/%zu张"), at + 1, G.count); }
    G.ninfo = n; (void)it;
}
static void bind_info(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; static const char *const INFO_ICONS[] = {"page", "folder", "clock", "grid", "storage", "info", "list"};
    unsigned at = index % LIST_SLOTS; int w = INK_CANVAS_W - 2 * INK_MARGIN; lv_obj_clean(slot);
    const char *icon = INFO_ICONS[index < 7 ? index : 6];
    if (!strcmp(G.info_labels[index], TR("尺寸"))) icon = "grid";
    else if (!strcmp(G.info_labels[index], TR("大小"))) icon = "storage";
    else if (!strcmp(G.info_labels[index], TR("格式"))) icon = "info";
    else if (!strcmp(G.info_labels[index], TR("第几张"))) icon = "list";
    if (index % LIST_SLOTS) ink_ui_group_divider(slot, 0, w);
    lv_obj_remove_flag(ink_ui_list_row(slot, icon, G.info_labels[index], G.info_values[index], 0, 0, w, true, NULL, NULL), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(slot, LV_OBJ_FLAG_CLICKABLE);
    snprintf(G.objects[at], sizeof G.objects[at], "info:%zu", index);
    *field = (ink_ui_field_spec_t){.id = G.objects[at], .role = "text", .label = G.info_labels[index], .value = G.info_values[index]};
    *action = (ink_ui_action_spec_t){.id = G.objects[at], .label = G.info_labels[index]};
}
static void info_create(lv_obj_t *root, void *arg) {
    (void)arg; ink_shell_statusbar(root, true); ink_shell_topbar(root, TR("信息"));
    info_rows(); memset(&G.info_position, 0, sizeof G.info_position);
    int y = INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP, h = INK_CANVAS_H - INK_MARGIN - y, row = ink_ui_row_double();
    int capacity = LV_MAX(1, LV_MIN(INFO_ROWS, (h - INK_GAP - INK_CONTROL) / (row + INK_GAP_M)));
    lv_obj_t *box = ink_ui_box(root, 0, y, INK_CANVAS_W, h, 0, INK_SURFACE_WHITE, false);
    const ink_ui_page_spec_t page = {.id = "PhotoInfo", .title = TR("信息"), .object = {"gallery:info", 1}, .actions = turns(), .action_count = 2};
    G.list = ink_ui_list_create(box, &page, &G.info_position, (unsigned)capacity, 1, row, TR("不详"), bind_info, NULL);
    if (G.list) ink_ui_list_refresh(G.list, G.ninfo);
}
static void list_destroy(void) { G.list = NULL; }

static size_t trash_order[INK_FS_INDEX_MAX] INK_PSRAM_BSS;
static void trash_line(const ink_gallery_item_t *it, char *when, size_t when_cap, char *left, size_t left_cap) { /* deleted when, days left */
    char day[16] = "", origin[INK_FS_PATH_MAX]; int y = 0, m = 0, d = 0, since = -1;
    left[0] = 0;
    if (trash_parts(it->path, day, sizeof day, origin, sizeof origin) && sscanf(day, "%d-%d-%d", &y, &m, &d) == 3) since = days_since(day);
    if (!m) { snprintf(when, when_cap, "%s", TR("删除日期不详")); snprintf(left, left_cap, "%s", TR("不会自动删除")); return; }
    snprintf(when, when_cap, TR("%d月%d日删除"), m, d);
    if (since >= 0 && ink_time_valid()) snprintf(left, left_cap, TR("还剩 %d 天"), TRASH_DAYS - since > 0 ? TRASH_DAYS - since : 0);
}
static void bind_trash(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % LIST_SLOTS; int w = INK_CANVAS_W - 2 * INK_MARGIN; lv_obj_clean(slot);
    const ink_gallery_item_t *it = &G.data->trash[trash_order[index]];
    char when[40], left[40]; trash_line(it, when, sizeof when, left, sizeof left);
    snprintf(G.labels[at], sizeof G.labels[at], "%s", it->name); snprintf(G.values[at], sizeof G.values[at], "%s%s%s", when, left[0] ? " · " : "", left);
    ink_jw_t j; ink_jw_init(&j, G.args[at], sizeof G.args[at]); ink_jw_raw(&j, "{\"path\":"); ink_jw_str(&j, it->path); ink_jw_raw(&j, "}");
    snprintf(G.objects[at], sizeof G.objects[at], "trash:%u", (unsigned)it->id);
    if (index % LIST_SLOTS) ink_ui_group_divider(slot, 0, w);
    lv_obj_remove_flag(ink_ui_list_row(slot, "trash", G.labels[at], when, 0, 0, w, true, NULL, NULL), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(slot, LV_OBJ_FLAG_CLICKABLE);
    int y = ink_ui_row_double(), bw = (w - INK_GAP_S) / 2;
    lv_obj_t *days = ink_ui_line(slot, left, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CIRCLE_SMALL + INK_GAP_XL, y - INK_GAP_XS, w - INK_CIRCLE_SMALL - INK_GAP_XL);
    lv_obj_set_style_text_color(days, ink_ui_secondary(), 0); y += ink_ui_line_height(INK_FONT_NOTE);
    ink_shell_bind(ink_ui_pill(slot, TR("恢复"), 0, y, bw, INK_CONTROL, false, NULL, NULL), "photo.restore", G.args[at]);
    ink_shell_bind(ink_ui_text_button(slot, TR("永久删除"), bw + INK_GAP_S, y, bw, NULL, NULL), "photo.delete_forever", G.args[at]);
    *field = (ink_ui_field_spec_t){.id = G.objects[at], .role = "text", .label = G.labels[at], .value = G.values[at], .object = {G.objects[at], 1}};
    *action = (ink_ui_action_spec_t){.id = G.objects[at], .label = TR("恢复"), .capability = "photo.restore", .args_json = G.args[at], .object = field->object, .enabled = true};
}
static int trash_newer(const void *a, const void *b) { /* the newest deletion day first, then the name */
    const ink_gallery_item_t *x = &G.data->trash[*(const size_t *)a], *y = &G.data->trash[*(const size_t *)b];
    char dx[16] = "", dy[16] = "", ox[INK_FS_PATH_MAX], oy[INK_FS_PATH_MAX];
    trash_parts(x->path, dx, sizeof dx, ox, sizeof ox); trash_parts(y->path, dy, sizeof dy, oy, sizeof oy);
    int r = strcmp(dy, dx); return r ? r : strcmp(x->name, y->name);
}
static void trash_create(lv_obj_t *root, void *arg) {
    (void)arg; ink_shell_statusbar(root, true); ink_shell_topbar(root, TR("最近删除"));
    size_t n = G.data ? G.data->ntrash : 0;
    for (size_t i = 0; i < n; i++) trash_order[i] = i;
    if (n) qsort(trash_order, n, sizeof *trash_order, trash_newer);
    if (n) { char label[48]; snprintf(label, sizeof label, TR("清空 %zu 张"), n); top_button(root, TR("最近删除"), label, TR("清空"), "gallery.empty_trash", NULL); }
    int y = INK_STATUS_BAR + INK_GAP_S + INK_CONTROL + INK_GAP_S;
    lv_obj_t *note = ink_ui_notice(root, TR("照片在这里留 30 天，之后自动删除"), y);
    lv_obj_update_layout(note); y += lv_obj_get_height(note) * INK_CANVAS_H / ink_ui_px(INK_CANVAS_H) + INK_GAP;
    int h = INK_CANVAS_H - INK_MARGIN - y, row = ink_ui_row_double() + ink_ui_line_height(INK_FONT_NOTE) + INK_CONTROL;
    int capacity = LV_MAX(1, LV_MIN(LIST_SLOTS, (h - INK_GAP - INK_CONTROL) / (row + INK_GAP_M)));
    lv_obj_t *box = ink_ui_box(root, 0, y, INK_CANVAS_W, h, 0, INK_SURFACE_WHITE, false);
    if (!n) { G.list = NULL; empty_state(box, TR("最近删除里没有照片"), NULL, NULL); }
    else {
        const ink_ui_page_spec_t page = {.id = "GalleryTrash", .title = TR("最近删除"), .object = {"gallery:trash", 1}, .actions = turns(), .action_count = 2};
        G.list = ink_ui_list_create(box, &page, &G.trash_position, (unsigned)capacity, 1, row, TR("最近删除里没有照片"), bind_trash, NULL);
        if (G.list) ink_ui_list_refresh(G.list, n);
    }
    load(0, 0);
}

/* ------------------------------------------------------------------ what the AI reads */

static int add_json(char *out, size_t cap, int n, const char *before, const char *extra) { /* extra goes in front of `before` */
    char *end = n > 0 ? strstr(out, before) : NULL;
    if (end && end > out && end[-1] == '[' && extra[0] == ',') extra++; /* the first item of an empty array */
    size_t len = strlen(extra);
    if (!end || (size_t)n + len >= cap) { out[0] = 0; return -1; }
    memmove(end + len, end, strlen(end) + 1); memcpy(end, extra, len); return n + (int)len;
}
static int gallery_snapshot(char *out, size_t cap) {
    if (!G.list) {
        const ink_ui_field_spec_t f[] = {{.id = "status", .role = "status", .label = TR("相册"), .value = G.data && G.data->err ? read_error(G.data->err) : TR("正在整理照片，好了会自动出来")}};
        const ink_ui_action_spec_t a[] = {{.id = "reload", .label = TR("重读照片"), .capability = "gallery.reload", .enabled = G.data && G.data->err}};
        const ink_ui_page_spec_t page = {.id = "Gallery", .title = TR("相册"), .object = {"gallery:photos", 1}, .fields = f, .field_count = 1, .actions = a, .action_count = 1};
        return ink_ui_semantics_snapshot(&page, out, cap);
    }
    int n = ink_ui_list_snapshot(G.list, out, cap);
    static const char *const VIEW_NAMES[] = {TR_NOOP("全部"), TR_NOOP("相簿"), TR_NOOP("日期")};
    char extra[512], seeing[160]; ink_jw_t w; ink_jw_init(&w, extra, sizeof extra);
    snprintf(seeing, sizeof seeing, "%s%s%s", TR(VIEW_NAMES[G.view]), G.line[0] ? " · " : "", G.line);
    ink_jw_raw(&w, ",{\"id\":\"view\",\"role\":\"status\",\"label\":"); ink_jw_str(&w, TR("正在看")); ink_jw_raw(&w, ",\"value\":"); ink_jw_str(&w, seeing); ink_jw_raw(&w, "}");
    n = n < 0 || w.overflow ? -1 : add_json(out, cap, n, "],\"actions\":[", extra);
    ink_jw_init(&w, extra, sizeof extra);
    ink_jw_raw(&w, ",{\"id\":\"camera\",\"label\":"); ink_jw_str(&w, TR("拍照")); ink_jw_raw(&w, ",\"capability\":\"app.open\",\"args_json\":\"{\\\"id\\\":\\\"camera\\\"}\"}");
    if (G.filter) { ink_jw_raw(&w, ",{\"id\":\"clear\",\"label\":"); ink_jw_str(&w, TR("清除")); ink_jw_raw(&w, ",\"capability\":\"gallery.find\",\"args_json\":\"{}\"}"); }
    return n < 0 || w.overflow ? -1 : add_json(out, cap, n, "],\"pagination\":", extra);
}
static int photo_snapshot(char *out, size_t cap) {
    const ink_gallery_item_t *it = item_of(G.photo); size_t at = index_of(G.photo);
    char where[96] = "", day[48] = "", index[32] = "", zoom[24], rot[24], dir[INK_FS_PATH_MAX];
    snprintf(dir, sizeof dir, "%.*s", (int)(base_name(G.path) - G.path - (G.path[0] ? 1 : 0)), G.path);
    if (dir[0]) snprintf(where, sizeof where, "%s", album_name(dir));
    if (it) day_text(it->modified, day, sizeof day);
    if (at < G.count) snprintf(index, sizeof index, TR("第%zu/%zu张"), at + 1, G.count);
    if (zoomed()) snprintf(zoom, sizeof zoom, TR("%d 倍"), G.v.zoom); else snprintf(zoom, sizeof zoom, "%s", TR("适合屏幕"));
    snprintf(rot, sizeof rot, TR("转了 %d 度"), G.v.rot * 90);
    const ink_ui_field_spec_t f[] = {
        {.id = "file", .role = "image", .label = TR("照片文件"), .value = G.path}, {.id = "name", .role = "text", .label = TR("当前照片"), .value = G.caption},
        {.id = "album", .role = "text", .label = TR("相簿"), .value = where}, {.id = "date", .role = "text", .label = TR("日期"), .value = day},
        {.id = "index", .role = "text", .label = TR("第几张"), .value = index}, {.id = "zoom", .role = "text", .label = TR("缩放"), .value = zoom},
        {.id = "rotation", .role = "text", .label = TR("旋转"), .value = rot}, {.id = "status", .role = "status", .label = TR("提示"), .value = G.message},
        {.id = "notice", .role = "status", .label = TR("照片"), .value = G.notice}
    };
    size_t n = 0; ink_ui_action_spec_t *a = G.photo_actions;
    bool ready = G.path[0] != 0;
    a[n++] = (ink_ui_action_spec_t){.id = "prev", .label = TR("上一张"), .capability = "photo.prev", .enabled = at < G.count && at > 0};
    a[n++] = (ink_ui_action_spec_t){.id = "next", .label = TR("下一张"), .capability = "photo.next", .enabled = at + 1 < G.count};
    a[n++] = (ink_ui_action_spec_t){.id = "zoom_in", .label = TR("放大"), .capability = "photo.zoom", .args_json = "{\"step\":\"in\"}", .enabled = !G.seen && G.v.zoom < 4};
    a[n++] = (ink_ui_action_spec_t){.id = "zoom_out", .label = TR("缩小"), .capability = "photo.zoom", .args_json = "{\"step\":\"out\"}", .enabled = zoomed()};
    a[n++] = (ink_ui_action_spec_t){.id = "rotate", .label = TR("旋转"), .capability = "photo.rotate", .args_json = "{\"dir\":\"right\"}", .enabled = !G.seen && ready};
    a[n++] = (ink_ui_action_spec_t){.id = "info", .label = TR("信息"), .capability = "photo.info", .enabled = ready};
    a[n++] = (ink_ui_action_spec_t){.id = "delete", .label = TR("删除"), .capability = "photo.trash", .enabled = ready};
    a[n++] = (ink_ui_action_spec_t){.id = "ask", .label = TR("问 AI 这张图"), .capability = "photo.ask", .enabled = ready && !G.seen};
    a[n++] = (ink_ui_action_spec_t){.id = "pan", .label = TR("移动"), .capability = "photo.pan", .args_json = "{\"dir\":\"down\"}", .enabled = zoomed()};
    a[n++] = (ink_ui_action_spec_t){.id = "undo", .label = TR("撤销"), .capability = "photo.restore", .args_json = G.undo_args, .enabled = G.undo_args[0] != 0};
    a[n++] = (ink_ui_action_spec_t){.id = "forever", .label = TR("永久删除"), .capability = "photo.delete_forever", .args_json = G.forever_args, .enabled = G.forever_args[0] != 0};
    a[n++] = (ink_ui_action_spec_t){.id = "back", .label = TR("返回"), .capability = "shell.back", .enabled = true};
    const ink_ui_page_spec_t page = {.id = "Photo", .title = TR("照片"), .object = {G.path, G.revision}, .fields = f, .field_count = 9, .actions = a, .action_count = n};
    return ink_ui_semantics_snapshot(&page, out, cap);
}
static int info_snapshot(char *out, size_t cap) {
    ink_ui_field_spec_t f[INFO_ROWS];
    for (size_t i = 0; i < G.ninfo; i++) f[i] = (ink_ui_field_spec_t){.id = G.info_labels[i], .role = "text", .label = G.info_labels[i], .value = G.info_values[i]};
    const ink_ui_action_spec_t a[] = {{.id = "back", .label = TR("返回"), .capability = "shell.back", .enabled = true}};
    const ink_ui_page_spec_t page = {.id = "PhotoInfo", .title = TR("信息"), .object = {G.path, G.revision}, .fields = f, .field_count = G.ninfo, .actions = a, .action_count = 1};
    return ink_ui_semantics_snapshot(&page, out, cap);
}
static int trash_snapshot(char *out, size_t cap) {
    if (!G.list) {
        const ink_ui_field_spec_t f[] = {{.id = "status", .role = "status", .label = TR("最近删除"), .value = TR("最近删除里没有照片")}};
        const ink_ui_page_spec_t page = {.id = "GalleryTrash", .title = TR("最近删除"), .object = {"gallery:trash", 1}, .fields = f, .field_count = 1};
        return ink_ui_semantics_snapshot(&page, out, cap);
    }
    int n = ink_ui_list_snapshot(G.list, out, cap);
    char extra[160]; ink_jw_t w; ink_jw_init(&w, extra, sizeof extra);
    ink_jw_raw(&w, ",{\"id\":\"empty\",\"label\":"); ink_jw_str(&w, TR("清空")); ink_jw_raw(&w, ",\"capability\":\"gallery.empty_trash\",\"args_json\":\"{}\"}");
    return n < 0 || w.overflow ? -1 : add_json(out, cap, n, "],\"pagination\":", extra);
}
static bool gallery_key(ink_key_t k, bool lng) { return G.list && ink_ui_list_key(G.list, k, lng); }
static bool photo_key(ink_key_t k, bool lng) {
    if (lng || (k != INK_KEY_NEXT && k != INK_KEY_PREV)) return false;
    if (pan(0, k == INK_KEY_NEXT ? .5f : -.5f)) return true; /* zoomed: read it top to bottom first, like a page */
    ink_receipt_t rc; ink_cap_invoke(k == INK_KEY_NEXT ? "photo.next" : "photo.prev", NULL, INK_SRC_KEY, 0, &rc);
    return true;
}
static bool photo_swipe(ink_swipe_dir_t d) {
    if (zoomed() || (d != INK_SWIPE_LEFT && d != INK_SWIPE_RIGHT)) return false;
    ink_receipt_t rc; ink_cap_invoke(d == INK_SWIPE_LEFT ? "photo.next" : "photo.prev", NULL, INK_SRC_TOUCH, 0, &rc);
    return true;
}
static const ink_activity_t GALLERY = {.name = "Gallery", .app = "gallery", .create = gallery_create, .destroy = gallery_destroy, .snapshot = gallery_snapshot, .on_key = gallery_key};
static const ink_activity_t PHOTO = {.name = "Photo", .app = "gallery", .create = photo_create, .destroy = photo_destroy, .snapshot = photo_snapshot, .on_key = photo_key, .on_swipe = photo_swipe};
static const ink_activity_t INFO = {.name = "PhotoInfo", .app = "gallery", .create = info_create, .destroy = list_destroy, .snapshot = info_snapshot, .on_key = gallery_key};
static const ink_activity_t TRASH = {.name = "GalleryTrash", .app = "gallery", .create = trash_create, .destroy = list_destroy, .snapshot = trash_snapshot, .on_key = gallery_key};

/* ------------------------------------------------------------------ 最近删除 on the card (background executor) */

enum { PHOTO_CHANGED = -100 }; /* the photo on the screen is not the one on the card any more */
typedef struct { ink_exec_job_t job; ink_call_t *call; uint32_t epoch, id; int64_t size, mtime; int err; char from[INK_FS_PATH_MAX], to[INK_FS_PATH_MAX]; } move_t;
static void parent_of(const char *path, char *out, size_t cap) { snprintf(out, cap, "%.*s", (int)(base_name(path) - 1 - path), path); }
static void up(char *dir) { char *s = strrchr(dir, '/'); if (s) *s = 0; }
static void trash_done(void *arg);
static void trash_job(void *arg) { /* into <space>/最近删除/<today>/<where it was>, by a rename on the same card */
    move_t *m = arg; ink_fs_entry_t e; char day[16], dir[INK_FS_PATH_MAX];
    const char *space = !strncmp(m->from, "/me/", 4) ? "/me" : "/family";
    if (ink_time_valid()) ink_gallery_date_key(ink_time_now_s(), '-', day, sizeof day); else snprintf(day, sizeof day, "%s", UNDATED_DIR);
    if (snprintf(m->to, sizeof m->to, "%s/%s/%s%s", space, TRASH_DIR, day, m->from + strlen(space)) >= (int)sizeof m->to) m->err = INK_FS_ERR_PATH;
    else {
        uint32_t revision = 0; parent_of(m->to, dir, sizeof dir);
        m->err = ink_fs_mkdir(dir);
        if (!m->err) m->err = ink_fs_lookup(m->id, &e);
        if ((!m->err && (strcmp(e.path, m->from) || e.size != m->size || e.mtime != m->mtime)) || m->err == INK_FS_ERR_NOT_FOUND)
            m->err = PHOTO_CHANGED; /* not the photo on the screen any more */
        for (int i = 0; !m->err && !(revision = ink_fs_revision()) && i < 50; i++) osal_task_delay_us(20000);
        if (!m->err) m->err = revision ? ink_fs_move_if_revision(m->from, m->to, revision, NULL) : INK_FS_ERR_BUSY;
    }
    ink_exec_post(INK_EXEC_UI, &m->job, trash_done, m);
}
static const char *move_error(int err) {
    switch (err) {
    case PHOTO_CHANGED: return TR("这张照片刚刚变了，请重新打开");
    case INK_FS_ERR_PATH: return TR("这张照片的路径太长，放不进最近删除，只能永久删除");
    case INK_FS_ERR_NO_SPACE: return TR("存储卡满了，照片还在原处");
    case INK_FS_ERR_EXISTS: return TR("最近删除里已经有同名的照片，照片还在原处");
    case INK_FS_ERR_DENIED: return TR("没有权限挪这张照片");
    case INK_FS_ERR_NO_CARD: case INK_FS_ERR_USB: return read_error(err);
    default: return TR("存储卡读写出错，照片还在原处");
    }
}
static void trash_done(void *arg) {
    move_t *m = arg; ink_call_t *c = m->call;
    G.trashing[0] = 0;
    if (m->epoch == ink_member_epoch() && m->err) {
        c->rc.status = m->err == PHOTO_CHANGED ? INK_RC_STALE : INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "%s", ink_fs_err_name(m->err == PHOTO_CHANGED ? INK_FS_ERR_EXISTS : m->err));
        clear_status(); snprintf(G.message, sizeof G.message, "%s", move_error(m->err));
        if (m->err == INK_FS_ERR_PATH) { ink_jw_t w; ink_jw_init(&w, G.forever_args, sizeof G.forever_args); ink_jw_raw(&w, "{\"path\":"); ink_jw_str(&w, m->from); ink_jw_raw(&w, "}"); }
        if (G.body && !strcmp(ink_ui_page_name(), "Photo")) { build_body(); ink_ui_hint_obj(G.body, INK_REFRESH_UI); ink_ui_content_changed(); }
    } else if (m->epoch == ink_member_epoch()) {
        ink_args_t undo; ink_args_init(&undo); ink_args_str(&undo, "path", m->to);
        snprintf(c->rc.object, sizeof c->rc.object, "photo:%u", (unsigned)m->id);
        ink_cap_undoable(c, "photo.restore", &undo, UNDO_MS);
        snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("已移到最近删除"));
        size_t at = index_of(m->id); bool here = G.body && !strcmp(ink_ui_page_name(), "Photo");
        if (at < G.count) { memmove(G.ids + at, G.ids + at + 1, (G.count - at - 1) * sizeof *G.ids); G.count--; }
        if (here && !G.count) ink_ui_pop(); /* that was the last one: back to the (now empty) list */
        else if (here) { /* the next one, with the line that says where this one went and how to get it back */
            clear_status(); snprintf(G.message, sizeof G.message, "%s", TR("已移到最近删除"));
            ink_jw_t w; ink_jw_init(&w, G.undo_args, sizeof G.undo_args); ink_jw_raw(&w, "{\"path\":"); ink_jw_str(&w, m->to); ink_jw_raw(&w, "}");
            photo_open_id(G.ids[at < G.count ? at : G.count - 1]);
        }
    }
    ink_cap_done(c); osal_free(m);
}
static bool restore_target(const char *origin, char *out, size_t cap) { /* where it was, or "name（恢复）.png" beside it */
    ink_fs_stat_t st;
    for (int k = 0; k < 10; k++) {
        const char *ext = strrchr(base_name(origin), '.'); int stem = ext ? (int)(ext - origin) : (int)strlen(origin);
        int n = !k ? snprintf(out, cap, "%s", origin) : k == 1 ? snprintf(out, cap, "%.*s%s%s", stem, origin, TR("（恢复）"), ext ? ext : "")
                                                              : snprintf(out, cap, TR("%.*s（恢复%d）%s"), stem, origin, k, ext ? ext : "");
        if (n < 0 || n >= (int)cap) return false;
        if (ink_fs_stat(out, &st) == INK_FS_ERR_NOT_FOUND) return true;
    }
    return false;
}
static void restored(void *arg);
static void cap_restore(ink_call_t *c) { /* background executor */
    char path[INK_FS_PATH_MAX], day[16], origin[INK_FS_PATH_MAX], dir[INK_FS_PATH_MAX];
    move_t *m = osal_calloc(1, sizeof *m, OSAL_MEM_EXTERNAL);
    if (!m) { c->rc.status = INK_RC_FAILED; return; }
    ink_args_get_str(&c->args, "path", path, sizeof path);
    int r = !trash_parts(path, day, sizeof day, origin, sizeof origin) || !is_image(path) ? INK_FS_ERR_PATH : 0;
    if (!r && !restore_target(origin, m->to, sizeof m->to)) r = INK_FS_ERR_EXISTS;
    if (!r) { parent_of(m->to, dir, sizeof dir); r = ink_fs_mkdir(dir); }
    if (!r) r = ink_fs_move(path, m->to);
    if (!r) { parent_of(path, dir, sizeof dir); while (in_trash(dir) && !ink_fs_remove(dir)) up(dir); } /* empty folders go */
    if (r) {
        c->rc.status = r == INK_FS_ERR_PATH ? INK_RC_INVALID : INK_RC_FAILED; snprintf(c->rc.detail, sizeof c->rc.detail, "%s", ink_fs_err_name(r));
        snprintf(m->from, sizeof m->from, "%s", r == INK_FS_ERR_EXISTS ? TR("原来的地方同名的照片太多，照片还在最近删除里") : move_error(r));
    } else {
        uint32_t cursor = 0; ink_fs_entry_t e; parent_of(m->to, dir, sizeof dir);
        while (ink_fs_index(dir, INK_FS_IMAGE, &cursor, &e) == 1) if (!strcmp(e.path, m->to)) { m->id = e.id; break; }
        char json[INK_FS_PATH_MAX + 32]; ink_jw_t w; ink_jw_init(&w, json, sizeof json); ink_jw_raw(&w, "{\"path\":"); ink_jw_str(&w, m->to); ink_jw_raw(&w, "}");
        ink_cap_result(c, json); snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("照片已恢复"));
    }
    m->err = r; m->epoch = ink_member_epoch();
    if (ink_exec_post(INK_EXEC_UI, &m->job, restored, m)) osal_free(m);
}
static void restored(void *arg) { /* the photo page shows the photo again; the lists follow file.changed */
    move_t *m = arg;
    if (m->epoch == ink_member_epoch() && ink_ui_page_current()) {
        const char *page = ink_ui_page_name();
        if (m->err && (!strcmp(page, "Photo") || !strcmp(page, "GalleryTrash"))) {
            clear_status(); snprintf(G.message, sizeof G.message, "%s", m->from);
            if (!strcmp(page, "Photo")) photo_changed_view(); else ink_ui_redraw();
        } else if (!m->err && !strcmp(page, "Photo")) { clear_status(); load(0, m->id); }
    }
    osal_free(m);
}
static bool photo_path(const char *path) { return (!strncmp(path, "/me/", 4) || !strncmp(path, "/family/", 8)) && is_image(path) && !strstr(path, "/../"); }
static ink_rc_t name_for_card(ink_args_t *args, ink_receipt_t *rc, ink_source_t src) {
    (void)src; char path[INK_FS_PATH_MAX];
    if (ink_args_get_str(args, "path", path, sizeof path) < 0 || !photo_path(path)) { snprintf(rc->detail, sizeof rc->detail, "not a photo"); return INK_RC_INVALID; }
    ink_args_str(args, "name", base_name(path)); return INK_RC_DONE;
}
static void cap_delete_forever(ink_call_t *c) { /* background executor, after the person said yes */
    char path[INK_FS_PATH_MAX]; ink_args_get_str(&c->args, "path", path, sizeof path);
    int r = photo_path(path) ? ink_fs_remove(path) : INK_FS_ERR_PATH;
    if (!r) { char dir[INK_FS_PATH_MAX]; parent_of(path, dir, sizeof dir); while (in_trash(dir) && !ink_fs_remove(dir)) up(dir); }
    if (r) { c->rc.status = r == INK_FS_ERR_PATH ? INK_RC_INVALID : INK_RC_FAILED; snprintf(c->rc.detail, sizeof c->rc.detail, "%s", ink_fs_err_name(r)); }
}
/* One tree at a time (the file service runs one job): every expired day, or everything for 清空. */
typedef struct { ink_call_t *call; bool all; char member[16]; } sweep_t;
static bool sweep_next(sweep_t *s);
static void sweep_ended(void *ctx, const ink_fs_job_status_t *st) {
    sweep_t *s = ctx;
    if (st->state == INK_FS_JOB_DONE && sweep_next(s)) return;
    if (s->call) { if (st->state != INK_FS_JOB_DONE) s->call->rc.status = INK_RC_FAILED; ink_cap_done(s->call); }
    osal_free(s);
}
static bool sweep_next(sweep_t *s) {
    const char *spaces[] = {"/me", ink_member_current()->role != INK_ROLE_CHILD ? "/family" : NULL};
    if (strcmp(s->member, ink_member_current()->id)) return false;
    for (int k = 0; k < 2 && spaces[k]; k++) {
        char root[40], dir[INK_FS_PATH_MAX] = ""; ink_fs_dir_t *d = NULL; ink_fs_stat_t st;
        trash_root(spaces[k], root, sizeof root);
        if (s->all) { if (ink_fs_stat(root, &st)) continue; snprintf(dir, sizeof dir, "%s", root); }
        else if (!ink_fs_dir_open(root, &d)) {
            while (!dir[0] && ink_fs_dir_next(d, &st) == 1) {
                if (!st.is_dir) continue;
                int since = days_since(st.name);
                if (since >= TRASH_DAYS) snprintf(dir, sizeof dir, "%s/%s", root, st.name);
            }
            ink_fs_dir_close(d);
        }
        if (!dir[0]) continue;
        int id = ink_fs_delete_start(dir);
        if (id <= 0) return false; /* a job already runs: the next opening of the gallery tries again */
        ink_fs_job_on_end(id, sweep_ended, s); return true;
    }
    return false;
}
static void sweep(void *arg) {
    sweep_t *s = arg;
    if (!s->all && !ink_time_valid()) { osal_free(s); return; } /* no trustworthy date: nothing is old enough */
    if (sweep_next(s)) return;
    if (s->call) ink_cap_done(s->call);
    osal_free(s);
}
static ink_exec_job_t purge_job INK_PSRAM_BSS;
static void purge_start(void) { /* once per opening, never on a timer */
    sweep_t *s = osal_calloc(1, sizeof *s, OSAL_MEM_EXTERNAL);
    if (!s) return;
    snprintf(s->member, sizeof s->member, "%s", ink_member_current()->id);
    if (ink_exec_post(INK_EXEC_BG, &purge_job, sweep, s)) osal_free(s);
}
static void cap_empty_trash(ink_call_t *c) { /* background executor, after the person said yes */
    sweep_t *s = osal_calloc(1, sizeof *s, OSAL_MEM_EXTERNAL);
    if (!s) { c->rc.status = INK_RC_FAILED; return; }
    s->call = c; s->all = true; snprintf(s->member, sizeof s->member, "%s", ink_cap_call_member(c));
    ink_cap_defer(c); sweep(s);
}

/* ------------------------------------------------------------------ capabilities */

static void photo_found(void *ctx, const ink_receipt_t *rc) {
    ink_call_t *c = ctx;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int64_t id = 0; ink_args_get_int(&c->args, "id", &id);
    bool found = j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result)) &&
        ink_json_eq(j, ink_json_find(j, ink_json_find(j, 0, "entries") + 1, "type"), "image");
    osal_free(j);
    if (!found) c->rc.status = INK_RC_REJECTED;
    else if (strcmp(ink_cap_call_member(c), ink_member_current()->id) || !ink_member_space_open()) c->rc.status = INK_RC_STALE;
    else ink_ui_push(&PHOTO, (void *)(uintptr_t)id);
    ink_cap_done(c);
}
static struct { ink_svc_deadline_t at; photo_question_t *p; int waits; } photo_retry; /* one question waits at a time */
static void photo_question_ready(void *ctx, const ink_receipt_t *rc);
static void photo_question_again(void *ctx) { /* timer task: the thumbnail once more (a member switch fails the epoch check) */
    photo_question_t *p = ctx; ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", p->photo);
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_UI, .done = photo_question_ready, .ctx = p};
    if (ink_cap_call("file.thumb", &a, &o)) osal_free(p);
}
static void photo_question_ready(void *ctx, const ink_receipt_t *rc) {
    photo_question_t *p = ctx;
    if (photo_retry.p == p) photo_retry.p = NULL;
    /* 0 means the files are busy (an index batch), not a new version: as read_photos, try again shortly */
    if (!ink_fs_revision() && !photo_retry.p && photo_retry.waits++ < 50) {
        photo_retry.p = p;
        ink_svc_at(&photo_retry.at, osal_now_us() + 100000ull, photo_question_again, p);
        return;
    }
    if (p->epoch == ink_member_epoch() && p->generation == ink_ui_generation() && p->photo == G.photo &&
        p->revision && p->revision == ink_fs_revision() && ink_ui_page_current() && !strcmp(ink_ui_page_name(), "Photo") && rc->status == INK_RC_DONE && rc->result)
        ink_ai_ask_photo(rc->result);
    osal_free(p);
}
static void photo_step(ink_call_t *c, int delta) {
    size_t at = index_of(G.photo);
    if (at >= G.count || (delta < 0 && !at) || (delta > 0 && at + 1 >= G.count)) { c->rc.status = INK_RC_INVALID; snprintf(c->rc.detail, sizeof c->rc.detail, "no %s photo", delta < 0 ? "previous" : "next"); return; }
    clear_status(); photo_open_id(G.ids[at + (size_t)delta]);
}
static void gallery_action(ink_call_t *c) {
    if (!strcmp(c->cap->id, "gallery.reload")) { ink_ui_redraw(); return; }
    if (!strcmp(c->cap->id, "photo.ask")) {
        photo_question_t *p = osal_malloc(sizeof *p, OSAL_MEM_EXTERNAL);
        if (!p || !G.path[0] || !G.revision || G.revision != ink_fs_revision()) { osal_free(p); c->rc.status = INK_RC_STALE; return; }
        *p = (photo_question_t){ink_member_epoch(), ink_ui_generation(), G.photo, ink_fs_revision()};
        photo_retry.waits = 0;
        ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", G.photo);
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_UI, .done = photo_question_ready, .ctx = p};
        if (ink_cap_call("file.thumb", &a, &o)) { osal_free(p); c->rc.status = INK_RC_FAILED; }
        return;
    }
    if (!strcmp(c->cap->id, "photo.open")) {
        /* photo.open already passed the app policy; the service still checks file ownership. */
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_UI, .done = photo_found, .ctx = c};
        ink_cap_defer(c);
        if (ink_cap_call("file.lookup", &c->args, &o)) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
        return;
    }
    if (!G.list) { c->rc.status = INK_RC_INVALID; return; }
    G.position.selected[0] = 0;
    if (!ink_ui_list_turn(G.list, !strcmp(c->cap->id, "gallery.next") ? 1 : -1)) return;
    if (G.month_line) { line_text(); lv_label_set_text(G.month_line, G.line); ink_ui_hint_obj(G.month_line, INK_REFRESH_UI); }
}
static void browse_action(ink_call_t *c) { /* gallery.show, gallery.find */
    char s[INK_FS_PATH_MAX] = "";
    G.position.selected[0] = 0;
    if (!strcmp(c->cap->id, "gallery.show")) {
        ink_args_get_str(&c->args, "view", s, sizeof s);
        if (!strcmp(s, "trash")) { if (!ink_ui_push(&TRASH, NULL)) c->rc.status = INK_RC_FAILED; return; }
        int which = !strcmp(s, "albums") ? VIEW_ALBUMS : !strcmp(s, "months") ? VIEW_MONTHS : VIEW_ALL;
        if (which == VIEW_ALL) G.filter = FILTER_NONE;
        G.view = which; memset(&G.rows, 0, sizeof G.rows);
    } else {
        char month[16] = "";
        ink_args_get_str(&c->args, "album", s, sizeof s); ink_args_get_str(&c->args, "month", month, sizeof month);
        size_t a = 0; while (s[0] && G.data && a < G.data->nalbums && strcmp(G.data->albums[a].path, s)) a++;
        if (s[0] && (!G.data || a >= G.data->nalbums)) { c->rc.status = INK_RC_INVALID; snprintf(c->rc.detail, sizeof c->rc.detail, "no such album"); return; }
        int y = 0, m = 0;
        if (month[0] && strcmp(month, "undated") && (sscanf(month, "%4d-%2d", &y, &m) != 2 || strlen(month) != 7)) { c->rc.status = INK_RC_INVALID; return; }
        G.filter = s[0] ? FILTER_ALBUM : month[0] ? FILTER_MONTH : FILTER_NONE; G.album = (uint16_t)a;
        snprintf(G.month, sizeof G.month, "%.7s", month); G.view = VIEW_ALL;
        memset(&G.position, 0, sizeof G.position);
    }
    filtered(); view_changed();
}
static void photo_action(ink_call_t *c) { /* the shown photo: only while its page is up */
    char s[16] = ""; const char *id = c->cap->id;
    ink_args_get_str(&c->args, !strcmp(id, "photo.zoom") ? "step" : "dir", s, sizeof s);
    if (!strcmp(id, "photo.next") || !strcmp(id, "photo.prev")) { photo_step(c, !strcmp(id, "photo.next") ? 1 : -1); return; }
    if (!strcmp(id, "photo.info")) { if (!G.path[0] || !ink_ui_push(&INFO, NULL)) c->rc.status = INK_RC_STALE; return; }
    if (!G.path[0]) { c->rc.status = INK_RC_STALE; return; }
    if (!strcmp(id, "photo.trash")) {
        move_t *m = osal_calloc(1, sizeof *m, OSAL_MEM_EXTERNAL);
        if (!m || G.trashing[0]) { osal_free(m); c->rc.status = INK_RC_FAILED; return; }
        *m = (move_t){.call = c, .epoch = ink_member_epoch(), .id = G.photo, .size = G.size, .mtime = G.mtime};
        snprintf(m->from, sizeof m->from, "%s", G.path); snprintf(G.trashing, sizeof G.trashing, "%s", G.path);
        ink_cap_defer(c);
        if (ink_exec_post(INK_EXEC_BG, &m->job, trash_job, m)) { G.trashing[0] = 0; c->rc.status = INK_RC_FAILED; ink_cap_done(c); osal_free(m); }
        return;
    }
    if (G.seen) { c->rc.status = INK_RC_INVALID; snprintf(c->rc.detail, sizeof c->rc.detail, "this photo cannot be shown"); return; }
    if (!strcmp(id, "photo.zoom")) {
        int z = !strcmp(s, "in") ? (G.v.zoom < 4 ? G.v.zoom * 2 : 4) : !strcmp(s, "out") ? (G.v.zoom > 1 ? G.v.zoom / 2 : 1) : 1;
        zoom_to(z, z > 1 ? G.v.cx : -1, z > 1 ? G.v.cy : -1);
    } else if (!strcmp(id, "photo.pan")) {
        if (!pan(!strcmp(s, "left") ? -.5f : !strcmp(s, "right") ? .5f : 0, !strcmp(s, "up") ? -.5f : !strcmp(s, "down") ? .5f : 0)) c->rc.status = INK_RC_INVALID;
    } else { clear_status(); G.v.rot = (G.v.rot + (!strcmp(s, "left") ? 3 : 1)) % 4; G.v.cx = G.v.cy = -1; G.dw = G.dh = 0; request_render(); }
    snprintf(c->rc.object, sizeof c->rc.object, "file:%u", (unsigned)G.photo);
}
#define PATH_INPUT "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127}},\"required\":[\"path\"]}"
static const ink_cap_t GALLERY_CAPS[] = {
    {.id = "gallery.reload", .app = "gallery", .title = "手动重读当前家人的照片", .activity = "Gallery", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = gallery_action},
    {.id = "photo.ask", .app = "gallery", .title = "把当前有权查看的照片缩略图交给 AI", .activity = "Photo", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = gallery_action},
    {.id = "gallery.prev", .app = "gallery", .title = "相册上一页（照片、相簿、信息、最近删除的列表）", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = gallery_action},
    {.id = "gallery.next", .app = "gallery", .title = "相册下一页（照片、相簿、信息、最近删除的列表）", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = gallery_action},
    {.id = "photo.open", .locked_owner_only = true, .app = "gallery", .title = "查看有权访问的图片文件", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = gallery_action,
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}},\"required\":[\"id\"]}"},
    {.id = "gallery.show", .locked_owner_only = true, .app = "gallery", .title = "相册切到全部、相簿、日期，或打开最近删除", .activity = "Gallery", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = browse_action,
     .input = "{\"type\":\"object\",\"properties\":{\"view\":{\"type\":\"string\",\"enum\":[\"all\",\"albums\",\"months\",\"trash\"]}},\"required\":[\"view\"]}"},
    {.id = "gallery.find", .locked_owner_only = true, .app = "gallery", .title = "只看一个相簿（文件夹路径）或一个月（YYYY-MM 或 undated）的照片；都不给就清除筛选", .activity = "Gallery", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = browse_action,
     .input = "{\"type\":\"object\",\"properties\":{\"album\":{\"type\":\"string\",\"maxLength\":127},\"month\":{\"type\":\"string\",\"maxLength\":7}}}"},
    {.id = "photo.next", .locked_owner_only = true, .app = "gallery", .title = "同一筛选里的下一张照片", .activity = "Photo", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = photo_action},
    {.id = "photo.prev", .locked_owner_only = true, .app = "gallery", .title = "同一筛选里的上一张照片", .activity = "Photo", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = photo_action},
    {.id = "photo.info", .locked_owner_only = true, .app = "gallery", .title = "看这张照片的信息（拍摄时间、尺寸、大小、格式）", .activity = "Photo", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = photo_action},
    {.id = "photo.zoom", .locked_owner_only = true, .app = "gallery", .title = "照片放大、缩小（适合屏幕、2 倍、4 倍）", .activity = "Photo", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = photo_action,
     .input = "{\"type\":\"object\",\"properties\":{\"step\":{\"type\":\"string\",\"enum\":[\"in\",\"out\",\"fit\"]}},\"required\":[\"step\"]}"},
    {.id = "photo.pan", .locked_owner_only = true, .app = "gallery", .title = "放大后把照片挪半屏", .activity = "Photo", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = photo_action,
     .input = "{\"type\":\"object\",\"properties\":{\"dir\":{\"type\":\"string\",\"enum\":[\"left\",\"right\",\"up\",\"down\"]}},\"required\":[\"dir\"]}"},
    {.id = "photo.rotate", .locked_owner_only = true, .app = "gallery", .title = "照片转 90 度看（不改文件）", .activity = "Photo", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = photo_action,
     .input = "{\"type\":\"object\",\"properties\":{\"dir\":{\"type\":\"string\",\"enum\":[\"left\",\"right\"]}},\"required\":[\"dir\"]}"},
    {.id = "photo.trash", .locked_owner_only = true, .app = "gallery", .title = "把这张照片移到最近删除（30 天内能恢复）", .activity = "Photo", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = photo_action},
    {.id = "photo.restore", .locked_owner_only = true, .app = "gallery", .title = "把最近删除里的照片放回原处（按路径）", .effect = INK_FX_LOCAL_WRITE, .child_ok = true,
     .exec = INK_EXEC_BG, .qos = INK_QOS_BACKGROUND, .input = PATH_INPUT, .run = cap_restore},
    {.id = "photo.delete_forever", .locked_owner_only = true, .app = "gallery", .title = "永久删除一张照片（不能恢复）", .ask = TR_NOOP("永久删除这张照片（{name}），不能恢复"),
     .effect = INK_FX_LOCAL_DESTRUCTIVE, .ask_by_hand = true, .child_ok = true, .exec = INK_EXEC_BG, .qos = INK_QOS_BACKGROUND, .input = PATH_INPUT,
     .prepare_confirm = name_for_card, .run = cap_delete_forever},
    {.id = "gallery.empty_trash", .locked_owner_only = true, .app = "gallery", .title = "清空最近删除（不能恢复）", .ask = TR_NOOP("清空最近删除里的全部照片，不能恢复"),
     .effect = INK_FX_LOCAL_DESTRUCTIVE, .ask_by_hand = true, .child_ok = true, .exec = INK_EXEC_BG, .qos = INK_QOS_BACKGROUND, .timeout_ms = 300000, .run = cap_empty_trash},
};
static bool relevant(const char *path, const char *kind) { /* a change this page shows: an image here, or the whole card */
    if (!path[0] || !strcmp(path, "/") || !strcmp(kind, "rescan") || !strcmp(kind, "cleaned")) return true; /* the whole card: also the index getting ready */
    if (!is_image(path) && strchr(base_name(path), '.')) return false; /* another kind of file; a folder may hold photos */
    char me[32], family[40]; trash_root("/me", me, sizeof me); trash_root("/family", family, sizeof family);
    if (!strcmp(ink_ui_page_name(), "GalleryTrash")) return in_trash(path) || !strcmp(path, me) || !strcmp(path, family);
    return (under(path, "/me") && !in_trash(path) && strcmp(path, me)) || under(path, LOCK_ALBUM);
}
static void photos_changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    if (!ink_ui_page_current()) return;
    char path[INK_FS_PATH_MAX] = "", kind[24] = ""; ink_args_get_str(args, "path", path, sizeof path); ink_args_get_str(args, "kind", kind, sizeof kind);
    const char *page = ink_ui_page_name();
    if ((!strcmp(page, "Gallery") || !strcmp(page, "GalleryTrash")) && relevant(path, kind)) load(0, 0);
    else if (!strcmp(page, "Photo") && !strcmp(path, G.path) && !strcmp(kind, "removed") && strcmp(path, G.trashing)) ink_ui_pop();
}
static void card_state(void *ctx, int h, const char *id) { /* the card came, went or was lent: read again by itself */
    (void)ctx; (void)h;
    if (!ink_ui_page_current() || (strcmp(ink_ui_page_name(), "Gallery") && strcmp(ink_ui_page_name(), "GalleryTrash"))) return;
    /* one watch for both (the shared watch table is nearly full); the index getting ready comes as file.changed "/" */
    if (!strcmp(id, "storage.card") || !strcmp(id, "storage.mode")) load(0, 0); /* not storage.free */
}
int ink_gallery_init(void) {
    static const ink_app_def_t app = {.id = "gallery", .title = TR_NOOP("相册"), .icon = INK_UI_ICON(gallery), .entry = &GALLERY, .system_app = true, .child_ok = true};
    return ink_cap_register(GALLERY_CAPS, sizeof GALLERY_CAPS / sizeof *GALLERY_CAPS) || ink_app_register(&app) ||
           ink_cap_subscribe("file.changed", photos_changed, NULL, INK_EXEC_UI) ||
           ink_state_watch("storage.", card_state, NULL, INK_EXEC_UI) ? -1 : 0;
}
