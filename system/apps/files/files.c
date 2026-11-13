/* File pages (files-3): Amaze's and Fossify's flows adapted to e-ink (D-140, docs/tasks/files-3/对照表.md). The file
 * menu, favourites and browse events run Flipper's archive code (vendor/flipper, through files_flipper.c). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "ink/osal.h"
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/exec.h"
#include "ink/sys/file.h"
#include "ink/sys/image.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_list.h"
#include "ink/sys/data.h"
#include "ink/sys/i18n.h"
#include "ink/ui_shell.h"
#include "files_flipper.h"

enum { HOME, LIST, ACTIONS, MOVE, NAME, EDIT, SORT, SEARCH, BATCH, PAGE_COUNT, ACTION_MAX = 12, CONFLICT_MAX = 32, FAIL_MAX = 20 };
enum { B_CHECK, B_SPACE, B_ASK, B_RUN, B_DONE };
typedef struct { char path[INK_FS_PATH_MAX], name[INK_FS_NAME_MAX]; uint32_t id; int64_t size, mtime; bool dir, direct, fav; ink_fs_type_t type; } item_t;
typedef struct { uint32_t id; char name[INK_FS_NAME_MAX]; int64_t size, mtime, esize, emtime; uint8_t choice; } conflict_t;
static const ink_activity_t PAGES[PAGE_COUNT];
static const ink_db_def_t DRAFT_REFS = {.name = "files.drafts", .title = "文本文件草稿引用", .scope = INK_DB_MEMBER, .schema = 1, .json = true, .ai_read = true, .max_records = INK_FS_INDEX_MAX};
static const char *const SORTS[] = {"name", "time", "size", "type"}, *const CHOICES[] = {"keep_both", "skip", "replace"};
static struct {
    item_t items[INK_FS_INDEX_MAX], selected; uint16_t view[INK_FS_INDEX_MAX]; size_t count, rows, capacity;
    ink_ui_list_t *list; ink_ui_list_position_t position, depth_pos[10];
    ink_ui_page_spec_t model; ink_ui_action_spec_t actions[ACTION_MAX]; int events[ACTION_MAX]; ink_ui_field_spec_t fields[4];
    lv_obj_t *screen, *notice, *field, *preview, *progress, *list_body, *buttons[ACTION_MAX], *slots[4], *images[4];
    lv_image_dsc_t thumbs[4]; uint8_t pixels[4][INK_ICON_LIST * INK_ICON_LIST]; ink_exec_job_t job, job_ui;
    char member[16], path[INK_FS_PATH_MAX], dest[INK_FS_PATH_MAX], loaded[INK_FS_PATH_MAX], back_from[INK_FS_PATH_MAX], name_dir[INK_FS_PATH_MAX];
    char cursor[16], preview_text[129], query[64], empty[160], where[224], details[224], stop_args[32];
    char ids[4][32], args[4][384], names[4][INK_FS_NAME_MAX + 8], values[4][240], marks[4][16], crumbs[10][192];
    char rename[576], target[576], source[288], object[32], deleting[INK_FS_PATH_MAX], renaming[INK_FS_PATH_MAX], renamed_from[INK_FS_PATH_MAX];
    uint32_t epoch, serial, selected_id, index_cursor, found, fav_hash;
    bool known, multi, new_text, all_marked, stale, truncated, from_fav, open_found, previewing, busy, indexing, copy, all, expect_receipt;
    uint32_t marked[INK_FS_INDEX_MAX], draft, ref; size_t marked_count;
    char edit_path[INK_FS_PATH_MAX], body[4096], chunk[2049], batch[1024], selection[INK_ARGS_TEXT], result[4096];
    conflict_t conf[CONFLICT_MAX]; size_t conf_n, conf_at, conf_total, fail_n, total, succeeded, skipped, failed, not_done;
    int64_t need, free_bytes; uint64_t shown_us; int bstate, task, mode, sort, error; bool desc;
    char fails[FAIL_MAX][INK_FS_NAME_MAX + 48], renamed[INK_FS_NAME_MAX * 2];
} F INK_PSRAM_BSS;
_Static_assert(9 <= ACTION_MAX, "file toolbar action count");

static const char *base(const char *path) { const char *s = strrchr(path, '/'); return s ? s + 1 : path; }
static bool under(const char *path, const char *root) { size_t n = strlen(root); return !strncmp(path, root, n) && (!path[n] || path[n] == '/'); }
static bool root_dir(const char *path) { return !strcmp(path, "/me") || !strcmp(path, "/family") || (!strncmp(path, "/members/", 9) && !strchr(path + 9, '/')); }
static const char *directory(void) { return F.mode == MOVE ? F.dest : F.path; }
static bool writable(const char *path) { return ink_member_current()->role == INK_ROLE_ADULT || under(path, "/me"); }
static bool join(char *out, size_t cap, const char *dir, const char *name) { return snprintf(out, cap, "%s/%s", dir, name) < (int)cap; }
static void status(const char *text) {
    char count[64]; snprintf(count, sizeof count, TR("已选 %zu 个文件"), F.marked_count);
    if (F.notice) lv_label_set_text(F.notice, !text[0] && F.multi && F.mode == LIST ? F.marked_count ? count : TR("先选文件") : text);
    ink_ui_content_changed();
}
static bool current(void *ctx) { return (uint32_t)((uintptr_t)ctx >> 3) == F.serial && ink_ui_page_current() && F.epoch == ink_member_epoch(); }
static void request(const char *cap, const ink_args_t *args, ink_cap_done_fn done, unsigned slot) {
    ink_call_opts_t opts = {.src = INK_SRC_APP, .member = F.member, .reply_exec = INK_EXEC_UI, .done = done, .ctx = (void *)(((uintptr_t)F.serial << 3) | slot)};
    if (ink_cap_call(cap, args, &opts)) { F.busy = false; status(TR("暂时读不了文件，请返回后重试")); }
}
static void pair_json(char *out, size_t cap, const char *key, const char *value, const char *key2, const char *value2) {
    ink_jw_t w; ink_jw_init(&w, out, cap); ink_jw_raw(&w, "{"); ink_jw_str(&w, key); ink_jw_raw(&w, ":"); ink_jw_str(&w, value);
    if (key2) { ink_jw_raw(&w, ","); ink_jw_str(&w, key2); ink_jw_raw(&w, ":"); ink_jw_str(&w, value2); } ink_jw_raw(&w, "}");
    if (w.overflow) out[0] = 0;
}
/* The places a path goes through, from its root: labels and where each one ends. */
static int crumbs(const char *path, char labels[][INK_FS_NAME_MAX], size_t *ends, int max) {
    size_t at = !strncmp(path, "/members/", 9) ? 9 + strcspn(path + 9, "/") : under(path, "/me") ? 3 : under(path, "/family") ? 7 : 0;
    if (!at) return 0;
    if (at > 7) {
        char id[16]; const char *name = id; snprintf(id, sizeof id, "%.*s", (int)(at - 9), path + 9);
        for (int i = 0; i < INK_MEMBERS_MAX; i++) { const ink_member_t *m = ink_member_at(i); if (m && !strcmp(m->id, id)) name = m->name; }
        snprintf(labels[0], INK_FS_NAME_MAX, TR("%s的文件"), name);
    } else snprintf(labels[0], INK_FS_NAME_MAX, "%s", at == 3 ? TR("我的") : TR("家庭"));
    int n = 0; ends[n++] = at;
    while (path[at] == '/' && n < max) { size_t len = strcspn(path + at + 1, "/"); snprintf(labels[n], INK_FS_NAME_MAX, "%.*s", (int)len, path + at + 1); at += 1 + len; ends[n++] = at; }
    if (n > 1 && ends[1] == strlen("/me/收件箱") && !strncmp(path, "/me/收件箱", ends[1])) snprintf(labels[1], INK_FS_NAME_MAX, "%s", TR("传进来的"));
    return n;
}
static void location(char *out, size_t cap, const char *path, size_t len) {
    char p[INK_FS_PATH_MAX], labels[10][INK_FS_NAME_MAX]; size_t ends[10]; snprintf(p, sizeof p, "%.*s", (int)len, path); out[0] = 0;
    int n = crumbs(p, labels, ends, 10);
    for (int i = 0; i < n; i++) { size_t k = strlen(out); snprintf(out + k, cap - k, "%s%s", i ? " › " : "", labels[i]); }
}
static void when(char *out, size_t cap, int64_t t) {
    ink_tm_t a, b;
    if (t < 1577836800) { snprintf(out, cap, "%s", TR("时间未知")); return; } /* a clock never set: before 2020 */
    ink_time_to_local(t, &a); ink_time_to_local(ink_time_now_s(), &b);
    if (a.year == b.year && a.month == b.month && a.day == b.day) snprintf(out, cap, TR("今天 %d:%02d"), a.hour, a.min);
    else if (a.year == b.year) snprintf(out, cap, TR("%d月%d日"), a.month, a.day);
    else snprintf(out, cap, TR("%d年%d月%d日"), a.year, a.month, a.day);
}
static void size_text(char *out, size_t cap, int64_t size) {
    if (size >= 1024 * 1024) snprintf(out, cap, "%.1f MB", (double)size / (1024 * 1024));
    else if (size >= 1024) snprintf(out, cap, "%.1f KB", (double)size / 1024);
    else snprintf(out, cap, TR("%lld 字节"), (long long)size);
}
static const char *type_name(int type) {
    static const char *const kinds[] = {TR_NOOP("文本"), TR_NOOP("书"), TR_NOOP("音频"), TR_NOOP("图片"), TR_NOOP("压缩包"), TR_NOOP("文件夹"), TR_NOOP("其它")};
    return TR(kinds[type >= 0 && type <= FILES_TYPE_UNKNOWN ? type : FILES_TYPE_UNKNOWN]);
}
/* Names compare with their numbers as numbers (第2章 before 第10章); other characters by code, ASCII case-free. */
static int natural(const char *a, const char *b) {
    while (*a && *b) {
        if (*a >= '0' && *a <= '9' && *b >= '0' && *b <= '9') {
            while (*a == '0') a++;
            while (*b == '0') b++;
            size_t na = strspn(a, "0123456789"), nb = strspn(b, "0123456789"); int c = na != nb ? (na < nb ? -1 : 1) : strncmp(a, b, na);
            if (c) return c;
            a += na; b += nb;
        } else {
            int ca = *a >= 'A' && *a <= 'Z' ? *a + 32 : (unsigned char)*a, cb = *b >= 'A' && *b <= 'Z' ? *b + 32 : (unsigned char)*b;
            if (ca != cb) return ca - cb;
            a++; b++;
        }
    }
    return (unsigned char)*a - (unsigned char)*b;
}
static int compare(const void *x, const void *y) {
    const item_t *a = &F.items[*(const uint16_t *)x], *b = &F.items[*(const uint16_t *)y];
    if (a->dir != b->dir) return a->dir ? -1 : 1;
    size_t la = strlen(a->path) - strlen(a->name), lb = strlen(b->path) - strlen(b->name);
    int c = F.mode == SEARCH ? strncmp(a->path, b->path, la < lb ? la : lb) : 0;
    if (c || (F.mode == SEARCH && la != lb)) return c ? c : la < lb ? -1 : 1;
    int64_t d = F.sort == 1 ? a->mtime - b->mtime : F.sort == 2 ? a->size - b->size : 0;
    const char *ea = strrchr(a->name, '.'), *eb = strrchr(b->name, '.');
    c = d ? (d < 0 ? -1 : 1) : F.sort == 3 ? strcasecmp(ea ? ea : "", eb ? eb : "") : 0;
    if (!c) c = natural(a->name, b->name);
    if (!c) c = strcmp(a->path, b->path);
    return F.desc ? -c : c;
}
static bool query_ok(void) { bool wide = false; for (const char *p = F.query; *p; p++) wide |= (unsigned char)*p >= 0x80; return wide ? F.query[0] : strlen(F.query) >= 2; }
static bool matches(const char *name) { size_t n = strlen(F.query); for (const char *p = name; *p; p++) if (!strncasecmp(p, F.query, n)) return true; return false; }
static void build_view(void) {
    F.rows = 0;
    for (size_t i = 0; i < F.count; i++) if (F.mode == SEARCH ? !F.items[i].dir && query_ok() && matches(F.items[i].name) : F.items[i].direct) F.view[F.rows++] = (uint16_t)i;
    if (directory()[0]) qsort(F.view, F.rows, sizeof *F.view, compare);
}
static void sort_prefs(void) { files_prefs_t *p = files_prefs_begin(); F.sort = p->sort >= 0 && p->sort < 4 ? p->sort : 0; F.desc = p->desc; files_prefs_end(false); }

static void operation_done(void *ctx, const ink_receipt_t *rc);
static bool moved_to(const char *json, char *to);
static void clicked(lv_event_t *e) {
    unsigned index = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    const ink_ui_action_spec_t *a = &F.actions[index];
    if (!a->enabled || !ink_ui_page_current() || F.epoch != ink_member_epoch()) return;
    if (F.events[index]) { files_fl_select(F.selected.path, F.selected.dir, F.from_fav); files_fl_event(F.events[index]); return; } /* Flipper's menu events */
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .member = F.member, .args_json = a->args_json, .reply_exec = INK_EXEC_UI, .done = operation_done, .ctx = (void *)((uintptr_t)F.serial << 3)};
    if (!strcmp(a->id, "rename") && !F.selected.dir && moved_to(F.rename, F.renaming)) memcpy(F.renamed_from, F.selected.path, sizeof F.renamed_from); /* see changed() */
    if (ink_cap_call(a->capability, NULL, &opts)) status(TR("没能完成，请重试"));
}
static lv_obj_t *button(unsigned i, lv_obj_t *screen, const char *label, const char *cap, const char *args, int x, int y, int width, bool primary) {
    F.actions[i] = (ink_ui_action_spec_t){.id = cap, .label = label, .capability = cap, .args_json = args, .enabled = true};
    return F.buttons[i] = ink_ui_pill(screen, label, x, y, width, INK_CONTROL, primary, clicked, (void *)(uintptr_t)i);
}
static void enable(unsigned i, bool on) { F.actions[i].enabled = on; if (F.buttons[i]) lv_obj_set_state(F.buttons[i], LV_STATE_DISABLED, !on); }
/* Written buttons from the bottom edge, as many to a row as their text allows. The top of the first row. */
static int toolbar(unsigned first, unsigned n, const char *const t[][3]) {
    int w = INK_CANVAS_W - 2 * INK_MARGIN, need = INK_CONTROL;
    for (unsigned i = 0; i < n; i++) { int tw = ink_ui_text_width(t[i][0], INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP_S; if (tw > need) need = tw; }
    unsigned per = (unsigned)((w + INK_GAP_S) / (need + INK_GAP_S)); per = !per ? 1 : per > n ? n : per;
    unsigned rows = (n + per - 1) / per; int y = INK_CANVAS_H - INK_MARGIN - (int)rows * INK_CONTROL - (int)(rows - 1) * INK_GAP_S;
    for (unsigned i = 0; i < n; i++) {
        unsigned r = i / per, m = r + 1 < rows ? per : n - r * per; int cw = (w - (int)(m - 1) * INK_GAP_S) / (int)m;
        button(first + i, F.screen, t[i][0], t[i][1], t[i][2], INK_MARGIN + (int)(i % per) * (cw + INK_GAP_S), y + (int)r * (INK_CONTROL + INK_GAP_S), cw, false);
    }
    F.model.action_count = first + n; return y;
}
static void renamed(lv_event_t *e) {
    (void)e; char path[INK_FS_PATH_MAX], parent[INK_FS_PATH_MAX];
    snprintf(parent, sizeof parent, "%s", F.mode == NAME ? F.name_dir : F.selected.path); if (F.mode != NAME) *strrchr(parent, '/') = 0;
    const char *name = ink_textfield_text(F.field);
    bool valid = name[0] && name[0] != '.' && !strchr(name, '/') && join(path, sizeof path, parent, name);
    if (F.mode == NAME) { pair_json(F.rename, sizeof F.rename, "path", valid ? path : "", F.new_text ? "text" : NULL, ""); status(valid ? "" : TR("请输入名称，不能以点开头或含 /")); }
    else pair_json(F.rename, sizeof F.rename, "from", F.selected.path, "to", valid ? path : "");
    if (F.mode == NAME && F.new_text && (strlen(name) <= 4 || strcmp(name + strlen(name) - 4, ".txt"))) { valid = false; status(TR("文本文件名必须以 .txt 结尾")); }
    F.actions[1].enabled = valid && writable(F.mode == NAME ? F.name_dir : F.selected.path); ink_ui_content_changed();
}
static void searched(lv_event_t *e) { (void)e; pair_json(F.target, sizeof F.target, "query", ink_textfield_text(F.field), NULL, NULL); ink_ui_content_changed(); }
static void editor_open(void) {
    ink_db_t *db = ink_db_open_member(&DRAFT_REFS, F.member); ink_rec_t rec; ink_db_cursor_t cursor = {0}; ink_json_t *j = ink_app_alloc(sizeof *j, true);
    char json[384], path[INK_FS_PATH_MAX]; F.draft = F.ref = 0;
    while (db && j && ink_db_next(db, &cursor, &rec, json, sizeof json - 1) == 1) {
        if (rec.len >= sizeof json) continue;
        json[rec.len] = 0; int64_t draft = 0;
        if (!ink_json_parse(j, json, rec.len) && ink_json_str(j, ink_json_find(j, 0, "path"), path, sizeof path) > 0 && !strcmp(path, F.edit_path)) {
            ink_json_int(j, ink_json_find(j, 0, "draft"), &draft); F.ref = rec.id; F.draft = (uint32_t)draft; break;
        }
    }
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .initial = F.body, .max = sizeof F.body - 1, .draft_page = "FileEdit", .draft_id = F.draft};
    F.field = db ? ink_textfield_long_create(F.screen, &cfg) : NULL;
    if (F.field) {
        ink_textfield_style(F.field, INK_FIELD_BODY, TR("草稿（写回才会更新文件）"));
        F.draft = ink_text_draft_id(); ink_jw_t w; ink_jw_init(&w, json, sizeof json); ink_jw_raw(&w, "{\"path\":"); ink_jw_str(&w, F.edit_path); ink_jw_raw(&w, ",\"draft\":"); ink_jw_int(&w, F.draft); ink_jw_raw(&w, "}");
        ink_db_write_t put = {.id = F.ref, .data = json, .len = w.len}; if (ink_db_put(db, &put, &F.ref)) status(TR("草稿引用未能保存")); ink_db_flush(db);
        pair_json(F.target, sizeof F.target, "path", F.edit_path, "page", "FileEdit"); size_t n = strlen(F.target);
        snprintf(F.target + n - 1, sizeof F.target - n + 1, ",\"draft\":%u,\"text\":\"\",\"replace\":true}", (unsigned)F.draft);
        button(0, F.screen, TR("更新文件"), "file.write", F.target, INK_CANVAS_W - INK_MARGIN - 2 * INK_PRIMARY, INK_STATUS_BAR, 2 * INK_PRIMARY, true); F.model.action_count = 1;
    } else status(TR("暂时打不开文本编辑器"));
    ink_app_free(j); ink_db_close(db); F.busy = false;
}
static void edit_read(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx)) return;
    ink_json_t *j = ink_app_alloc(sizeof *j, true); size_t n = strlen(F.body); char *chunk = F.chunk;
    if (!j || rc->status != INK_RC_DONE || !rc->result || ink_json_parse(j, rc->result, strlen(rc->result)) || ink_json_str(j, ink_json_find(j, 0, "text"), chunk, sizeof F.chunk) < 0) { ink_app_free(j); operation_done(ctx, rc); return; }
    if (n + strlen(chunk) >= sizeof F.body) { ink_app_free(j); F.busy = false; status(TR("文本超过 4095 字节，无法编辑")); return; }
    memcpy(F.body + n, chunk, strlen(chunk) + 1); bool end = ink_json_eq(j, ink_json_find(j, 0, "end"), "true"); ink_app_free(j);
    if (end) editor_open(); else { ink_args_t a; ink_args_init(&a); ink_args_str(&a, "path", F.edit_path); ink_args_int(&a, "offset", strlen(F.body)); request("file.read", &a, edit_read, 0); }
}
static void selection_args(void) {
    size_t files = 0; for (size_t i = 0; i < F.count; i++) files += F.items[i].direct && !F.items[i].dir; F.all_marked = files && F.marked_count == files;
    ink_jw_t w; ink_jw_init(&w, F.selection, sizeof F.selection); ink_jw_raw(&w, "[");
    for (size_t i = 0; i < F.marked_count; i++) { if (i) ink_jw_raw(&w, ","); ink_jw_int(&w, F.marked[i]); } ink_jw_raw(&w, "]");
    if (w.overflow) F.selection[0] = 0;
}
/* The files this batch is about: the marked ones (all of the folder's: all_path) or the one in the menu. */
static void batch_args(bool destination) {
    ink_jw_t w; ink_jw_init(&w, F.batch, sizeof F.batch); ink_jw_raw(&w, "{");
    if (!F.multi) { ink_jw_raw(&w, "\"ids\":["); ink_jw_int(&w, F.selected.id); ink_jw_raw(&w, "]"); }
    else if (F.all_marked && !F.selection[0]) { ink_jw_raw(&w, "\"all_path\":"); ink_jw_str(&w, F.path); }
    else { ink_jw_raw(&w, "\"ids\":"); ink_jw_raw(&w, F.selection[0] ? F.selection : "[]"); }
    if (destination) { ink_jw_raw(&w, ",\"op\":"); ink_jw_str(&w, F.copy ? "copy" : "move"); ink_jw_raw(&w, ",\"to\":"); ink_jw_str(&w, F.dest); }
    ink_jw_raw(&w, "}"); if (w.overflow) F.batch[0] = 0;
}
static void opened(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx)) return;
    ink_json_t *j = ink_app_alloc(sizeof *j, true);
    if (j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        ink_json_str(j, ink_json_find(j, 0, "text"), F.preview_text, sizeof F.preview_text);
        if (F.preview) lv_label_set_text(F.preview, F.preview_text);
        status(TR("仅预览文件开头"));
    } else operation_done(ctx, rc);
    ink_app_free(j);
}
static void thumb_done(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx) || rc->status != INK_RC_DONE || !rc->result) return;
    unsigned at = (uintptr_t)ctx & 7; ink_json_t *j = ink_app_alloc(sizeof *j, true); int64_t w = 0, h = 0;
    if (j && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        ink_json_int(j, ink_json_find(j, 0, "width"), &w); ink_json_int(j, ink_json_find(j, 0, "height"), &h);
        int t = ink_json_find(j, 0, "gray4");
        if (t >= 0 && w > 0 && h > 0 && w <= INK_THUMB_EDGE && h <= INK_THUMB_EDGE && j->tok[t].end - j->tok[t].start == w * h && F.images[at]) {
            const char *gray = j->text + j->tok[t].start;
            for (unsigned y = 0; y < INK_ICON_LIST; y++) for (unsigned x = 0; x < INK_ICON_LIST; x++) {
                char c = gray[(y * (unsigned)h / INK_ICON_LIST) * (unsigned)w + x * (unsigned)w / INK_ICON_LIST];
                F.pixels[at][y * INK_ICON_LIST + x] = (uint8_t)((c <= '9' ? c - '0' : c - 'a' + 10) * 17);
            }
            F.thumbs[at] = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_L8, .w = INK_ICON_LIST, .h = INK_ICON_LIST, .stride = INK_ICON_LIST}, .data_size = sizeof F.pixels[at], .data = F.pixels[at]};
            lv_image_set_src(F.images[at], &F.thumbs[at]); lv_obj_remove_flag(F.images[at], LV_OBJ_FLAG_HIDDEN); ink_ui_content_changed();
        }
    }
    ink_app_free(j);
}
static void thumbnail(unsigned at, uint32_t id) { ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", id); request("file.thumb", &a, thumb_done, at); }
static bool is_marked(uint32_t id) { for (size_t i = 0; i < F.marked_count; i++) if (F.marked[i] == id) return true; return false; }
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % 4; item_t *item = &F.items[F.view[index]]; char text[96], time[32];
    int left = INK_ICON_LIST + 2 * INK_GAP, room = INK_CANVAS_W - 2 * INK_MARGIN - left - INK_GAP;
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, left, INK_GAP_S, room);
        ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, left, INK_GAP_S + ink_ui_line_height(INK_FONT_LIST), room);
        static const char *const icons[] = {"folder", "book", "headphones", "camera"};
        for (unsigned i = 0; i < 4; i++) ink_ui_icon(slot, icons[i], INK_ICON_LIST, INK_ICON_LINE, INK_GAP, INK_GAP_L, false);
        lv_obj_t *im = lv_image_create(slot); lv_obj_set_pos(im, ink_ui_px(INK_GAP), ink_ui_px(INK_GAP_L));
        ink_ui_icon(slot, "check", INK_ICON_SMALL, INK_ICON_LINE, INK_CANVAS_W - 2 * INK_MARGIN - INK_ICON_SMALL - INK_GAP, INK_GAP_L, false);
    }
    unsigned icon = item->dir ? 0 : item->type == INK_FS_IMAGE ? 3 : item->type == INK_FS_AUDIO ? 2 : 1;
    for (unsigned i = 0; i < 4; i++) { lv_obj_t *obj = lv_obj_get_child(slot, (int)i + 2); if (i == icon) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN); }
    F.slots[at] = slot; F.images[at] = lv_obj_get_child(slot, 6); lv_obj_add_flag(F.images[at], LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *check = lv_obj_get_child(slot, 7), *title = lv_obj_get_child(slot, 0); lv_obj_add_flag(check, LV_OBJ_FLAG_HIDDEN);
    ink_ui_surface_style(slot, INK_SURFACE_STATIC, false);
    bool found = F.found && item->id == F.found;
    lv_obj_set_style_text_font(title, ink_ui_font_px(INK_FONT_LIST, found ? INK_WEIGHT_BOLD : INK_WEIGHT_MEDIUM), 0);
    char name[sizeof item->name]; /* a copy: gcc refuses snprintf from F into F */
    memcpy(name, item->name, sizeof name); name[sizeof name - 1] = 0;
    snprintf(F.names[at], sizeof F.names[at], "%s", name);
    const char *ext = strrchr(name, '.');
    if (ink_ui_text_width(F.names[at], INK_FONT_LIST, INK_WEIGHT_MEDIUM) > room)
        snprintf(F.names[at], sizeof F.names[at], "%.*s…%s", (int)ink_text_byte_offset(name, 7), name, ext && strlen(ext) < 12 ? ext : "");
    lv_label_set_text(title, F.names[at]);
    if (F.mode == SEARCH || item->fav) { location(F.where, sizeof F.where, item->path, strlen(item->path) - strlen(item->name) - 1); snprintf(text, sizeof text, item->fav ? TR("收藏 · %s") : TR("在 %s"), F.where); }
    else if (item->dir) snprintf(text, sizeof text, "%s", strcmp(F.names[at], item->name) ? TR("文件夹（名称已截断）") : directory()[0] ? TR("文件夹") : "");
    else { size_text(text, sizeof text, item->size); when(time, sizeof time, item->mtime); size_t k = strlen(text); snprintf(text + k, sizeof text - k, " · %s", time); }
    snprintf(F.values[at], sizeof F.values[at], "%s%s", found ? TR("找到 · ") : "", text);
    lv_label_set_text(lv_obj_get_child(slot, 1), F.values[at]);
    if (item->id) snprintf(F.ids[at], sizeof F.ids[at], "file:%u", (unsigned)item->id); else snprintf(F.ids[at], sizeof F.ids[at], "dir:%zu", index);
    if (item->dir) pair_json(F.args[at], sizeof F.args[at], "path", item->path, NULL, NULL);
    else if (item->fav) pair_json(F.args[at], sizeof F.args[at], "path", item->path, NULL, NULL);
    else if (F.mode == SEARCH) { char dir[INK_FS_PATH_MAX]; snprintf(dir, sizeof dir, "%.*s", (int)(strlen(item->path) - strlen(item->name) - 1), item->path); pair_json(F.args[at], sizeof F.args[at], "path", dir, NULL, NULL); size_t k = strlen(F.args[at]); if (k) snprintf(F.args[at] + k - 1, sizeof F.args[at] - k + 1, ",\"find\":%u}", (unsigned)item->id); }
    else snprintf(F.args[at], sizeof F.args[at], "{\"id\":%u}", (unsigned)item->id);
    *field = (ink_ui_field_spec_t){.id = F.ids[at], .role = item->dir ? "directory" : "file", .label = item->dir ? F.names[at] : item->name, .value = F.values[at], .object = {F.ids[at], F.epoch}};
    *action = (ink_ui_action_spec_t){.id = F.ids[at], .label = item->dir ? TR("打开文件夹") : F.mode == SEARCH ? TR("到它所在的文件夹") : TR("文件操作"), .capability = item->dir || F.mode == SEARCH ? "files.browse" : "files.select", .args_json = F.args[at], .object = field->object, .enabled = !F.busy};
    if (F.multi && F.mode == LIST && !item->dir) {
        snprintf(F.args[at], sizeof F.args[at], "{\"id\":%u}", (unsigned)item->id); action->capability = "files.mark"; action->label = TR("选择文件");
        lv_obj_set_width(title, ink_ui_px(room - INK_ICON_SMALL - INK_GAP));
        if (is_marked(item->id)) lv_obj_remove_flag(check, LV_OBJ_FLAG_HIDDEN);
        snprintf(F.marks[at], sizeof F.marks[at], "%s", is_marked(item->id) ? TR("已选择") : TR("未选择")); field->value = F.marks[at];
    }
    if (item->type == INK_FS_IMAGE && item->id) thumbnail(at, item->id);
}
/* The list area, once the folder is read (the frame came first: two refreshes for a new folder). */
static void shown(void) {
    static const char *const errors[] = {"", TR_NOOP("没有存储卡"), TR_NOOP("存储卡借给电脑了，拔线或在电脑上推出后自动回来"), TR_NOOP("正在整理文件编号，好了自动重读"), TR_NOOP("暂时读不了这个文件夹")};
    if (F.error) {
        F.actions[ACTION_MAX - 1] = (ink_ui_action_spec_t){.id = "reload", .label = TR("重新读取"), .capability = "files.browse", .args_json = F.target, .enabled = true};
        pair_json(F.target, sizeof F.target, "path", directory(), NULL, NULL); if (F.error == 4) F.model.action_count = ACTION_MAX;
        lv_obj_t *e = ink_ui_empty(F.list_body, TR(errors[F.error]), F.error == 4 ? TR("重新读取") : NULL, clicked, (void *)(uintptr_t)(ACTION_MAX - 1));
        lv_obj_set_y(e, 0); status(""); return;
    }
    build_view();
    size_t keep = 0; for (size_t i = 0; F.mode == LIST && i < F.marked_count; i++) for (size_t r = 0; r < F.rows; r++) if (F.items[F.view[r]].id == F.marked[i]) { F.marked[keep++] = F.marked[i]; break; }
    if (F.mode == LIST) { F.marked_count = keep; selection_args(); }
    if (F.open_found && !F.found) for (size_t i = 0; i < F.count; i++) if (!F.items[i].dir && !strcmp(F.items[i].path, F.selected.path)) F.found = F.items[i].id;
    for (size_t r = 0; r < F.rows && (F.found || F.back_from[0]); r++) {
        const item_t *it = &F.items[F.view[r]];
        if ((F.found && it->id == F.found) || (F.back_from[0] && it->dir && !strcmp(it->path, F.back_from))) {
            F.position.page = r / F.capacity;
            if (it->id) snprintf(F.position.selected, sizeof F.position.selected, "file:%u", (unsigned)it->id); else snprintf(F.position.selected, sizeof F.position.selected, "dir:%zu", r);
        }
    }
    F.back_from[0] = 0;
    ink_ui_page_spec_t list_model = F.model; list_model.action_count = 2;
    if (!F.list) F.list = ink_ui_list_create(F.list_body, &list_model, &F.position, (unsigned)F.capacity, 1, ink_ui_row_double(), F.empty, bind_item, NULL);
    if (!F.list) { status(TR("暂时打不开文件列表")); return; }
    status(F.truncated ? TR("只列出前 1024 个") : F.mode == SEARCH && F.query[0] && !query_ok() ? TR("至少输入 2 个字母或数字，或 1 个汉字") : "");
    char state[16] = ""; ink_state_get_str(ink_state_find("file.index_state"), state, sizeof state);
    if (F.mode == SEARCH && strcmp(state, "ready")) status(TR("刚放进来的文件可能要等几秒才搜得到"));
    ink_ui_list_refresh(F.list, F.rows);
    if (F.open_found) { F.open_found = false; for (size_t i = 0; i < F.count; i++) if (F.found && F.items[i].id == F.found) { F.selected = F.items[i]; F.selected_id = F.found; F.known = true; files_fl_select(F.items[i].path, false, true); files_fl_event(FILES_EV_MENU); return; } }
}
static void load(void);
static void listed(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx)) return;
    ink_json_t *j = ink_app_alloc(sizeof *j, true);
    bool ok = j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result));
    if (ok) for (int t = 2; t < j->ntok; t++) {
        if (j->tok[t].type != 1) continue;
        item_t item = {0}; int64_t id = 0; char type[12] = "";
        item.dir = ink_json_eq(j, ink_json_find(j, t, "dir"), "true"); ink_json_int(j, ink_json_find(j, t, "mtime"), &item.mtime);
        if (!F.indexing && item.dir) {
            ink_json_str(j, ink_json_find(j, t, "name"), item.name, sizeof item.name); item.direct = true;
            if (!join(item.path, sizeof item.path, directory(), item.name)) continue;
        } else if (F.indexing) {
            ink_json_str(j, ink_json_find(j, t, "path"), item.path, sizeof item.path);
            const char *name = strrchr(item.path, '/'); size_t len = strlen(directory());
            if (!name || !under(item.path, directory())) continue;
            item.direct = (size_t)(name - item.path) == len;
            snprintf(item.name, sizeof item.name, "%s", name + 1); ink_json_int(j, ink_json_find(j, t, "id"), &id); item.id = (uint32_t)id;
            ink_json_str(j, ink_json_find(j, t, "type"), type, sizeof type); item.type = !strcmp(type, "image") ? INK_FS_IMAGE : !strcmp(type, "audio") ? INK_FS_AUDIO : !strcmp(type, "text") ? INK_FS_TEXT : INK_FS_OTHER;
            ink_json_int(j, ink_json_find(j, t, "size"), &item.size);
        } else continue;
        if (F.count == INK_FS_INDEX_MAX) { F.truncated = true; break; }
        F.items[F.count++] = item;
    }
    int next = ok && !F.truncated ? ink_json_find(j, 0, "next") : -1;
    F.cursor[0] = 0; F.index_cursor = 0;
    if (next >= 0) { int64_t value = 0; if (F.indexing) { ink_json_int(j, next, &value); F.index_cursor = (uint32_t)value; } else ink_json_str(j, next, F.cursor, sizeof F.cursor); }
    ink_app_free(j);
    if (ok && next >= 0) { load(); return; }
    if (ok && !F.indexing && F.mode != MOVE) { F.indexing = true; load(); return; }
    F.busy = false; F.count = ok ? F.count : 0; F.stale = false;
    F.error = ok ? 0 : strstr(rc->detail, "no card") ? 1 : strstr(rc->detail, "lent") ? 2 : strstr(rc->detail, "busy") ? 3 : 4;
    snprintf(F.loaded, sizeof F.loaded, "%s", ok && F.mode == LIST ? F.path : "");
    shown();
}
static void load(void) {
    ink_args_t a; ink_args_init(&a); ink_args_str(&a, "path", directory()); ink_args_int(&a, "limit", 10);
    if (F.indexing) ink_args_int(&a, "cursor", F.index_cursor); else ink_args_str(&a, "cursor", F.cursor);
    request(F.indexing ? "file.index" : "file.list", &a, listed, 0);
}
/* The file menu: what Flipper's render_item_menu composes for the selection, as written buttons. */
static void menu_page(const char *type) {
    static const struct { const char *item, *label, *cap; int event; bool write; } MENU[] = {
        {"Run in app", TR_NOOP("打开"), NULL, FILES_EV_RUN, false}, {"Pin", TR_NOOP("收藏"), "files.fav", 0, false}, {"Unpin", TR_NOOP("取消收藏"), "files.fav", 0, false},
        {"Copy", TR_NOOP("拷贝"), "files.move", 0, false}, {"Move to", TR_NOOP("移动"), "files.move", 0, true}, {"Delete", TR_NOOP("删除"), "file.delete", FILES_EV_DELETE, true},
        {"Edit", TR_NOOP("编辑"), "files.edit", 0, true}, {"Read", TR_NOOP("用阅读打开"), "mogo.open", 0, false},
        {"Pin folder", TR_NOOP("收藏"), "files.fav", 0, false}, {"Unpin folder", TR_NOOP("取消收藏"), "files.fav", 0, false}};
    const char *items[FILES_MENU_MAX]; char size[32], time[32];
    files_fl_select(F.selected.path, F.selected.dir, F.from_fav); int n = files_fl_menu(items), kind = files_fl_type(F.selected.path, F.selected.dir);
    bool can = writable(F.selected.path) && !root_dir(F.selected.path), rename = false;
    for (int i = 0; i < n; i++) rename |= !strcmp(items[i], "Rename");
    pair_json(F.source, sizeof F.source, "path", F.selected.path, NULL, NULL);
    if (F.selected.dir) snprintf(F.object, sizeof F.object, "%s", "dir:current"); else snprintf(F.object, sizeof F.object, "file:%u", (unsigned)F.selected_id);
    location(F.where, sizeof F.where, F.selected.path, strlen(F.selected.path) - strlen(base(F.selected.path)) - 1);
    size_text(size, sizeof size, F.selected.size); when(time, sizeof time, F.selected.mtime);
    if (F.selected.dir) snprintf(F.details, sizeof F.details, TR("文件夹 · %zu 项 · %s"), F.rows, time); else snprintf(F.details, sizeof F.details, "%s · %s · %s", size, time, type_name(kind));
    F.fields[0] = (ink_ui_field_spec_t){.id = "name", .role = "textbox", .label = TR("名称"), .value = F.selected.name, .object = {F.object, F.epoch}};
    F.fields[1] = (ink_ui_field_spec_t){.id = "path", .role = "text", .label = TR("位置"), .value = F.where};
    F.fields[2] = (ink_ui_field_spec_t){.id = "details", .role = "text", .label = TR("详情"), .value = F.details};
    F.fields[3] = (ink_ui_field_spec_t){.id = "preview", .role = "text", .label = TR("文件开头"), .value = F.preview_text}; F.model.field_count = 4;
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_S, w = INK_CANVAS_W - 2 * INK_MARGIN;
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .initial = F.selected.name, .max = INK_FS_NAME_MAX - 1};
    if (rename && can) {
        F.field = ink_textfield_create(F.screen, &cfg, NULL, NULL, false); lv_obj_set_y(F.field, ink_ui_px(y));
        ink_textfield_style(F.field, INK_FIELD_SINGLE, TR("名称"));
        F.actions[0] = (ink_ui_action_spec_t){.id = "name", .label = TR("编辑名称"), .capability = "files.name", .enabled = true};
        ink_textfield_bind_short(F.field, &F.model, &F.fields[0], &F.actions[0]); lv_obj_add_event_cb(F.field, renamed, LV_EVENT_VALUE_CHANGED, NULL);
        y += ink_textfield_height(INK_FIELD_SINGLE) + INK_GAP_S;
        button(1, F.screen, TR("改名"), "file.move", F.rename, INK_MARGIN, y, w, false); F.actions[1].id = "rename"; renamed(NULL); y += INK_CONTROL + INK_GAP;
    } else { F.fields[0].role = "text"; ink_ui_line(F.screen, F.selected.name, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, y, w); y += ink_ui_line_height(INK_FONT_LIST) + INK_GAP_S; F.model.action_count = 2; }
    ink_ui_line(F.screen, F.where, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w); y += ink_ui_line_height(INK_FONT_NOTE);
    ink_ui_line(F.screen, F.details, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w); y += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S;
    F.preview = ink_ui_label(F.screen, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, w);
    if (type && !strcmp(type, "image")) { F.images[0] = lv_image_create(F.screen); lv_obj_set_pos(F.images[0], ink_ui_px(INK_MARGIN), ink_ui_px(y)); thumbnail(0, F.selected_id); }
    const char *t[ACTION_MAX][3]; unsigned k = 0; int events[ACTION_MAX] = {0};
    snprintf(F.args[0], sizeof F.args[0], "{\"id\":%u}", (unsigned)F.selected_id);
    for (int i = 0; i < n && k < ACTION_MAX - 3; i++) for (size_t m = 0; m < sizeof MENU / sizeof *MENU; m++) {
        if (strcmp(items[i], MENU[m].item) || (MENU[m].write && !can)) continue;
        const char *cap = MENU[m].cap, *args = !strcmp(MENU[m].item, "Copy") ? "{\"copy\":true}" : !strcmp(MENU[m].item, "Move to") || !strcmp(MENU[m].item, "Edit") ? NULL : F.source;
        if (MENU[m].event == FILES_EV_RUN) { cap = kind == FILES_TYPE_TEXT ? "files.preview" : kind == FILES_TYPE_AUDIO ? "media.play" : kind == FILES_TYPE_IMAGE ? "photo.open" : "mogo.open"; args = kind == FILES_TYPE_TEXT || kind == FILES_TYPE_IMAGE ? F.args[0] : F.source; }
        events[k] = MENU[m].event; t[k][0] = TR(MENU[m].label); t[k][1] = cap; t[k++][2] = args;
    }
    int top = toolbar(2, k, t); memcpy(F.events + 2, events, k * sizeof *events);
    F.notice = ink_ui_notice(F.screen, "", top - ink_ui_line_height(INK_FONT_NOTE) - INK_GAP_S);
    F.model.object = (ink_ui_object_ref_t){F.object, F.epoch};
    status(kind == FILES_TYPE_UNKNOWN || kind == FILES_TYPE_ZIP ? TR("这种文件暂时打不开") : F.result);
}
static void selected(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx)) return;
    ink_json_t *j = ink_app_alloc(sizeof *j, true); int64_t size = 0, mtime = 0;
    if (!j || rc->status != INK_RC_DONE || !rc->result || ink_json_parse(j, rc->result, strlen(rc->result))) { ink_app_free(j); F.busy = false; status(TR("找不到这个文件，请返回列表")); return; }
    int t = ink_json_find(j, 0, "entries") + 1; memset(&F.selected, 0, sizeof F.selected); F.selected.id = F.selected_id;
    ink_json_str(j, ink_json_find(j, t, "path"), F.selected.path, sizeof F.selected.path);
    ink_json_int(j, ink_json_find(j, t, "size"), &size); ink_json_int(j, ink_json_find(j, t, "mtime"), &mtime); F.selected.size = size; F.selected.mtime = mtime;
    snprintf(F.selected.name, sizeof F.selected.name, "%s", base(F.selected.path));
    char type[12]; ink_json_str(j, ink_json_find(j, t, "type"), type, sizeof type); ink_app_free(j); F.busy = false;
    menu_page(type);
}
static void pop_to(int mode) { for (int i = 0; i < 4 && F.mode != mode && F.mode != HOME; i++) ink_ui_pop(); }

/* The batch page: checked -> (space) -> one question per name taken -> running -> result. */
static void batch_done(void *ctx, const ink_receipt_t *rc);
static void batch_start(void) {
    bool replace = false; ink_jw_t w; char keep[INK_ARGS_TEXT] = "", skip[INK_ARGS_TEXT] = "", swap[INK_ARGS_TEXT] = "";
    char *lists[] = {keep, skip, swap};
    for (size_t i = 0; i < F.conf_n; i++) {
        char *l = lists[F.conf[i].choice]; size_t n = strlen(l);
        snprintf(l + n, INK_ARGS_TEXT - n, "%s%u", n ? "," : "", (unsigned)F.conf[i].id); replace |= F.conf[i].choice == 2;
    }
    batch_args(true); size_t n = strlen(F.batch);
    ink_jw_init(&w, F.batch + n - 1, sizeof F.batch - n + 1);
    ink_jw_raw(&w, ",\"conflict\":"); ink_jw_str(&w, F.all && F.conf_n ? F.conf[F.conf_n - 1].choice == 1 ? "skip" : "keep_both" : "fail");
    for (int k = 0; k < 3; k++) if (lists[k][0]) { ink_jw_raw(&w, k ? k == 1 ? ",\"skip_ids\":[" : ",\"replace_ids\":[" : ",\"keep_both_ids\":["); ink_jw_raw(&w, lists[k]); ink_jw_raw(&w, "]"); }
    ink_jw_raw(&w, "}");
    F.bstate = B_RUN; F.task = 0; F.fail_n = F.failed = F.skipped = F.not_done = F.succeeded = 0; F.renamed[0] = F.stop_args[0] = 0; F.expect_receipt = !replace;
    if (w.overflow || strlen(F.batch) >= INK_ARGS_TEXT - 16) { F.bstate = B_DONE; snprintf(F.result, sizeof F.result, "%s", TR("选的文件太多，请分几次做")); ink_ui_redraw(); return; }
    files_prefs_t *p = files_prefs_begin(); snprintf(p->last, sizeof p->last, "%s", F.dest); files_prefs_end(true);
    ink_ui_redraw();
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .member = F.member, .args_json = F.batch, .reply_exec = INK_EXEC_UI, .done = batch_done, .ctx = (void *)((uintptr_t)F.serial << 3)};
    if (ink_cap_call(replace ? "file.batch_replace" : "file.batch", NULL, &opts)) status(TR("没能完成，请重试"));
}
static void checked(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx)) return;
    ink_json_t *j = ink_app_alloc(sizeof *j, true); int64_t v = 0;
    F.conf_n = F.conf_at = 0; F.all = false; F.need = F.free_bytes = 0;
    if (!j || rc->status != INK_RC_DONE || !rc->result || ink_json_parse(j, rc->result, strlen(rc->result))) {
        snprintf(F.result, sizeof F.result, "%s [%d %s %.200s]", rc->say[0] ? rc->say : TR("没能检查目标文件夹，请返回后重试"), rc->status, rc->detail, F.batch); F.bstate = B_DONE; ink_app_free(j); ink_ui_redraw(); return;
    }
    int list = ink_json_find(j, 0, "conflicts");
    for (int t = list + 1; list >= 0 && t < j->ntok && j->tok[t].start < j->tok[list].end && F.conf_n < CONFLICT_MAX; t++) {
        if (j->tok[t].type != 1) continue;
        conflict_t *c = &F.conf[F.conf_n++]; memset(c, 0, sizeof *c);
        ink_json_int(j, ink_json_find(j, t, "id"), &v); c->id = (uint32_t)v; ink_json_str(j, ink_json_find(j, t, "name"), c->name, sizeof c->name);
        ink_json_int(j, ink_json_find(j, t, "size"), &c->size); ink_json_int(j, ink_json_find(j, t, "mtime"), &c->mtime);
        ink_json_int(j, ink_json_find(j, t, "existing_size"), &c->esize); ink_json_int(j, ink_json_find(j, t, "existing_mtime"), &c->emtime);
    }
    ink_json_int(j, ink_json_find(j, 0, "conflict_count"), &v); F.conf_total = (size_t)v;
    if (F.copy) ink_json_int(j, ink_json_find(j, 0, "bytes"), &F.need);
    ink_json_int(j, ink_json_find(j, 0, "free"), &F.free_bytes); ink_app_free(j);
    F.bstate = F.need > F.free_bytes ? B_SPACE : F.conf_n ? B_ASK : B_RUN;
    if (F.bstate == B_RUN) batch_start(); else ink_ui_redraw();
}
static const char *reason_text(const char *why) {
    static const char *const map[][2] = {{"the card is full", TR_NOOP("空间不够")}, {"not a valid path", TR_NOOP("名字不合法")}, {"busy", TR_NOOP("正在做别的拷贝")},
        {"not found", TR_NOOP("找不到了")}, {"not allowed for this member", TR_NOOP("没有权限")}, {"already exists", TR_NOOP("同名文件已存在")},
        {"the card is lent to a computer", TR_NOOP("存储卡借给电脑了")}, {"no card", TR_NOOP("没有存储卡")}};
    for (size_t i = 0; i < sizeof map / sizeof *map; i++) if (!strcmp(why, map[i][0])) return TR(map[i][1]);
    return TR("读写出错");
}
static void batch_done(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx) || F.mode != BATCH) return;
    if (rc->status == INK_RC_PENDING_CONFIRMATION) { status(TR("请在设备上确认替换")); return; }
    ink_json_t *j = ink_app_alloc(sizeof *j, true);
    if (j && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        int list = ink_json_find(j, 0, "renamed"); char name[INK_FS_NAME_MAX];
        for (int t = list + 1; list >= 0 && t < j->ntok && j->tok[t].start < j->tok[list].end; t++)
            if (ink_json_str(j, t, name, sizeof name) > 0 && strlen(F.renamed) + strlen(name) + 4 < sizeof F.renamed) { size_t n = strlen(F.renamed); snprintf(F.renamed + n, sizeof F.renamed - n, "%s%s", n ? "、" : "", name); }
    }
    ink_app_free(j);
    if (rc->status != INK_RC_DONE && rc->status != INK_RC_PARTIAL && !F.total) snprintf(F.result, sizeof F.result, "%s", rc->say[0] ? rc->say : TR("没能完成，请检查文件后重试"));
    if (F.bstate == B_RUN || F.expect_receipt) { F.bstate = B_DONE; F.expect_receipt = false; ink_ui_redraw(); }
}
static void batch_progress(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    if (F.epoch != ink_member_epoch() || F.mode != BATCH || F.bstate != B_RUN || !ink_ui_page_current() || strcmp(ink_ui_page_name(), PAGES[BATCH].name)) return;
    bool ok = false; char path[INK_FS_PATH_MAX], why[48]; int64_t done = 0, total = 0, skipped = 0, task = 0, not_done = 0;
    ink_args_get_bool(args, "ok", &ok); ink_args_get_str(args, "path", path, sizeof path); ink_args_get_str(args, "reason", why, sizeof why); ink_args_get_int(args, "done", &done);
    ink_args_get_int(args, "total", &total); ink_args_get_int(args, "skipped", &skipped); ink_args_get_int(args, "task", &task); ink_args_get_int(args, "not_done", &not_done);
    if (!F.task && task) { F.task = (int)task; snprintf(F.stop_args, sizeof F.stop_args, "{\"id\":%d}", F.task); }
    if (!done) return;
    if (!ok && why[0] && strcmp(why, "skipped") && F.failed++ < FAIL_MAX) snprintf(F.fails[F.fail_n++], sizeof F.fails[0], "%.63s：%s", base(path), reason_text(why));
    F.total = (size_t)total; F.skipped = (size_t)skipped; F.not_done = (size_t)not_done; F.succeeded = F.total - F.failed - F.skipped - F.not_done;
    char text[64]; snprintf(text, sizeof text, TR("已处理 %lld/%lld"), (long long)done, (long long)total);
    if (done >= total) { if (!F.expect_receipt) { F.bstate = B_DONE; ink_ui_redraw(); } return; }
    uint64_t now = osal_now_us();
    if (F.progress && now - F.shown_us >= 2000000) { F.shown_us = now; lv_label_set_text(F.progress, text); ink_ui_hint_obj(F.progress, INK_REFRESH_UI); ink_ui_content_changed(); }
}
static void bind_fail(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; (void)action;
    if (!lv_obj_get_child_count(slot)) ink_ui_label(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP, INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP);
    lv_label_set_text(lv_obj_get_child(slot, 0), F.fails[index]);
    snprintf(F.ids[index % 4], sizeof F.ids[0], "fail:%zu", index);
    *field = (ink_ui_field_spec_t){.id = F.ids[index % 4], .role = "text", .label = TR("失败"), .value = F.fails[index], .object = {F.ids[index % 4], F.epoch}};
}
static void batch_page(void) {
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_S, w = INK_CANVAS_W - 2 * INK_MARGIN; char a[32], b[32], line[192];
    F.notice = ink_ui_notice(F.screen, "", y);
    if (F.bstate == B_CHECK) { status(TR("正在检查同名文件")); batch_args(true); size_t n = strlen(F.batch); if (n) { snprintf(F.batch + n - 1, sizeof F.batch - n + 1, ",\"check\":true}"); }
        ink_call_opts_t opts = {.src = INK_SRC_APP, .member = F.member, .args_json = F.batch, .reply_exec = INK_EXEC_UI, .done = checked, .ctx = (void *)((uintptr_t)F.serial << 3)};
        if (!F.batch[0] || ink_cap_call("file.batch", NULL, &opts)) status(TR("选的文件太多，请分几次做")); return; }
    if (F.bstate == B_SPACE) {
        size_text(a, sizeof a, F.need); size_text(b, sizeof b, F.free_bytes); snprintf(line, sizeof line, TR("需要 %s，还剩 %s，放得下的先做，放不下的会列出来"), a, b);
        ink_ui_label(F.screen, line, INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_MARGIN, y, w); snprintf(F.details, sizeof F.details, "%s", line);
        F.fields[0] = (ink_ui_field_spec_t){.id = "space", .role = "text", .label = TR("空间不够"), .value = F.details}; F.model.field_count = 1;
        button(0, F.screen, TR("开始"), "files.start", NULL, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w, true); F.model.action_count = 1; return;
    }
    if (F.bstate == B_ASK) {
        const conflict_t *c = &F.conf[F.conf_at]; char time[32];
        snprintf(line, sizeof line, TR("第 %zu 个，共 %zu 个"), F.conf_at + 1, F.conf_total > F.conf_n ? F.conf_total : F.conf_n); status(line);
        ink_ui_line(F.screen, c->name, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_MARGIN, y + INK_LINE_NOTE + INK_GAP, w);
        size_text(a, sizeof a, c->esize); when(time, sizeof time, c->emtime); snprintf(F.details, sizeof F.details, TR("原有的：%s · %s"), a, time);
        size_text(b, sizeof b, c->size); when(time, sizeof time, c->mtime); snprintf(F.where, sizeof F.where, TR("新的：%s · %s"), b, time);
        int t = y + INK_LINE_NOTE + INK_GAP + ink_ui_line_height(INK_FONT_LIST) + INK_GAP_S;
        ink_ui_line(F.screen, F.details, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, t, w); ink_ui_line(F.screen, F.where, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, t + ink_ui_line_height(INK_FONT_NOTE), w);
        F.fields[0] = (ink_ui_field_spec_t){.id = "existing", .role = "text", .label = c->name, .value = F.details}; F.fields[1] = (ink_ui_field_spec_t){.id = "new", .role = "text", .label = c->name, .value = F.where}; F.model.field_count = 2;
        bool same = !strcmp(F.dest, F.multi ? F.path : F.loaded[0] ? F.loaded : F.path) && F.copy;
        const char *t3[][3] = {{TR("保留两个"), "files.conflict", "{\"choice\":\"keep_both\"}"}, {TR("跳过"), "files.conflict", "{\"choice\":\"skip\"}"}, {TR("替换"), "files.conflict", "{\"choice\":\"replace\"}"}};
        int top = toolbar(1, same ? 2 : 3, t3);
        F.actions[0] = (ink_ui_action_spec_t){.id = "all", .label = TR("后面的同名都这样"), .capability = "files.conflict", .args_json = F.all ? "{\"all\":false}" : "{\"all\":true}", .enabled = true};
        ink_ui_toggle(F.screen, TR("后面的同名都这样"), F.all, INK_MARGIN, top - INK_CONTROL - INK_GAP, w, clicked, (void *)0);
        return;
    }
    if (F.bstate == B_RUN) {
        snprintf(line, sizeof line, TR("已处理 %zu/%zu"), (size_t)0, F.multi ? F.marked_count : (size_t)1);
        F.progress = ink_ui_label(F.screen, line, INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_MARGIN, y + INK_LINE_NOTE + INK_GAP, w); F.shown_us = osal_now_us();
        button(0, F.screen, TR("停下"), "job.cancel", F.stop_args, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w, false); F.model.action_count = 1; return;
    }
    if (F.result[0]) status(F.result);
    else if (F.total && F.skipped == F.total) status(TR("都跳过了，没有改动"));
    else { snprintf(line, sizeof line, TR("成功 %zu 个，跳过 %zu 个，失败 %zu 个"), F.succeeded, F.skipped, F.failed); size_t n = strlen(line);
        if (F.not_done) snprintf(line + n, sizeof line - n, TR("，没做 %zu 个"), F.not_done); status(line); }
    int t = y + INK_LINE_NOTE + INK_GAP_S;
    if (F.renamed[0]) { snprintf(F.details, sizeof F.details, TR("保留两个的放成了：%s"), F.renamed); ink_ui_label(F.screen, F.details, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, t, w); t += 2 * ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S; }
    button(2, F.screen, TR("完成"), "files.up", NULL, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w, true);
    F.actions[0] = (ink_ui_action_spec_t){.id = "prev", .label = TR("上一页"), .capability = "files.prev"}; F.actions[1] = (ink_ui_action_spec_t){.id = "next", .label = TR("下一页"), .capability = "files.next"}; F.model.action_count = 3;
    if (!F.fail_n) return;
    int rows = (INK_CANVAS_H - INK_MARGIN - INK_CONTROL - INK_GAP - t - INK_PRIMARY - INK_GAP) / (2 * ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S + INK_GAP_M);
    F.capacity = rows < 1 ? 1 : rows > 4 ? 4 : (size_t)rows;
    F.list_body = ink_ui_box(F.screen, 0, t, INK_CANVAS_W, (int)F.capacity * (2 * ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S + INK_GAP_M) + INK_PRIMARY + INK_GAP, 0, INK_SURFACE_WHITE, false);
    ink_ui_page_spec_t list_model = F.model; list_model.action_count = 2;
    F.list = ink_ui_list_create(F.list_body, &list_model, &F.position, (unsigned)F.capacity, 1, 2 * ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S, "", bind_fail, NULL);
    if (F.list) ink_ui_list_refresh(F.list, F.fail_n);
}

static void rescan_ui(void *arg) { if ((uint32_t)(uintptr_t)arg == F.serial && F.mode == HOME && ink_ui_page_current()) ink_ui_redraw(); }
static uint32_t fav_hash(void) { uint32_t h = 5381; files_prefs_t *p = files_prefs_begin(); for (const char *s = p->fav; *s; s++) h = h * 33 + (unsigned char)*s; files_prefs_end(false); return h; }
static void rescan_bg(void *arg) { uint32_t before = fav_hash(); files_fl_rescan(); if (fav_hash() != before) ink_exec_post(INK_EXEC_UI, &F.job_ui, rescan_ui, arg); }
static void home_items(void) {
    F.count = 0; files_prefs_t *p = files_prefs_begin();
    for (char *s = F.mode == HOME || F.mode == MOVE ? p->fav : (char *)""; *s && F.count < FILES_FAV_MAX; s += strcspn(s, "\n") + (s[strcspn(s, "\n")] == '\n')) {
        item_t *it = &F.items[F.count]; size_t n = strcspn(s, "\n"); memset(it, 0, sizeof *it);
        snprintf(it->path, sizeof it->path, "%.*s", (int)n, s); it->dir = n && it->path[n - 1] == '/'; if (it->dir) it->path[n - 1] = 0;
        if (!it->path[0] || (F.mode == MOVE && (!it->dir || !writable(it->path)))) continue;
        snprintf(it->name, sizeof it->name, "%s", base(it->path)); it->fav = it->direct = true; F.count++;
    }
    files_prefs_end(false);
    if (F.mode != MOVE || writable("/family")) { F.items[F.count] = (item_t){.path = "/family", .dir = true, .direct = true}; snprintf(F.items[F.count++].name, sizeof F.items[0].name, "%s", TR("家庭")); }
    F.items[F.count] = (item_t){.path = "/me", .dir = true, .direct = true}; snprintf(F.items[F.count++].name, sizeof F.items[0].name, "%s", TR("我的"));
    if (ink_member_current()->role == INK_ROLE_ADULT) for (int i = 0; i < INK_MEMBERS_MAX; i++) {
        const ink_member_t *m = ink_member_at(i); if (!m) break; if (!strcmp(m->id, F.member)) continue;
        item_t *item = &F.items[F.count++]; memset(item, 0, sizeof *item); item->dir = item->direct = true;
        snprintf(item->path, sizeof item->path, "/members/%s", m->id); snprintf(item->name, sizeof item->name, TR("%s的文件"), m->name);
    }
}
/* A list area between top and the bottom row(s), with the notice line just above them. */
static void list_area(int top, int bottom, bool loading) {
    int pitch = ink_ui_row_double() + INK_GAP_M, room = bottom - INK_GAP_S - ink_ui_line_height(INK_FONT_NOTE) - INK_GAP_S - top - INK_PRIMARY - INK_GAP;
    F.capacity = room / pitch < 1 ? 1 : room / pitch > 4 ? 4 : (size_t)(room / pitch);
    if (!F.notice) F.notice = ink_ui_notice(F.screen, "", bottom - INK_GAP_S - ink_ui_line_height(INK_FONT_NOTE));
    F.list_body = ink_ui_box(F.screen, 0, top, INK_CANVAS_W, (int)F.capacity * pitch + INK_PRIMARY + INK_GAP, 0, INK_SURFACE_WHITE, false);
    if (F.mode != LIST && F.mode != SEARCH) F.loaded[0] = 0; /* the items belong to this page now */
    if (loading) { F.busy = true; F.count = 0; F.truncated = F.indexing = false; F.cursor[0] = 0; F.index_cursor = 0; status(TR("正在读取文件夹")); load(); }
}
static void crumb_row(int y) {
    char labels[10][INK_FS_NAME_MAX]; size_t ends[10]; int n = crumbs(F.path, labels, ends, 10), first = n, used = 0, w = INK_CANVAS_W - 2 * INK_MARGIN;
    int sep = ink_ui_text_width(" › ", INK_FONT_LIST, INK_WEIGHT_MEDIUM), more = ink_ui_text_width("… › ", INK_FONT_LIST, INK_WEIGHT_MEDIUM), widths[10];
    for (int i = n - 1; i >= 0; i--) { widths[i] = ink_ui_text_width(labels[i], INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP_XS; if (widths[i] < INK_CONTROL) widths[i] = INK_CONTROL;
        if (used + widths[i] + (i ? more : 0) > w && i != n - 1) break; used += widths[i] + (i < n - 1 ? sep : 0); first = i; }
    int x = INK_MARGIN, ly = y + (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2;
    if (first > 0) { ink_ui_line(F.screen, "… ›", INK_FONT_LIST, INK_WEIGHT_MEDIUM, x, ly, more); x += more; }
    for (int i = first; i < n; i++) {
        char path[INK_FS_PATH_MAX]; snprintf(path, sizeof path, "%.*s", (int)ends[i], F.path); pair_json(F.crumbs[i], sizeof F.crumbs[i], "path", path, NULL, NULL);
        int cw = widths[i] < INK_CANVAS_W - INK_MARGIN - x ? widths[i] : INK_CANVAS_W - INK_MARGIN - x;
        ink_shell_bind(ink_ui_text_button(F.screen, labels[i], x, y, cw, NULL, NULL), "files.browse", F.crumbs[i]); x += cw;
        if (i < n - 1) { ink_ui_line(F.screen, "›", INK_FONT_LIST, INK_WEIGHT_MEDIUM, x, ly, sep); x += sep; }
    }
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg; F.serial++; F.error = 0; F.busy = false; F.screen = screen; F.list = NULL; F.list_body = F.notice = F.field = F.preview = F.progress = NULL; F.preview_text[0] = 0;
    memset(F.buttons, 0, sizeof F.buttons); memset(F.events, 0, sizeof F.events); memset(F.images, 0, sizeof F.images);
    F.mode = 0; while (F.mode < PAGE_COUNT - 1 && strcmp(PAGES[F.mode].name, ink_ui_page_name())) F.mode++;
    if (F.epoch != ink_member_epoch()) { memset(&F.position, 0, sizeof F.position); memset(F.depth_pos, 0, sizeof F.depth_pos); memset(&F.selected, 0, sizeof F.selected); F.path[0] = F.dest[0] = F.result[0] = F.edit_path[0] = F.loaded[0] = F.query[0] = F.back_from[0] = 0; F.multi = F.all_marked = F.open_found = false; F.marked_count = 0; F.selected_id = F.found = 0; F.epoch = ink_member_epoch(); }
    snprintf(F.member, sizeof F.member, "%s", ink_member_current()->id); sort_prefs();
    if (F.mode == HOME) { F.path[0] = 0; F.multi = false; F.marked_count = 0; F.found = 0; F.from_fav = false; memset(&F.position, 0, sizeof F.position); }
    memset(F.actions, 0, sizeof F.actions); memset(F.fields, 0, sizeof F.fields);
    static const char *const titles[] = {TR_NOOP("文件"), "", TR_NOOP("文件"), TR_NOOP("放到哪里？"), "", TR_NOOP("编辑文本"), TR_NOOP("排序"), TR_NOOP("搜索"), TR_NOOP("同名文件")};
    char title[INK_FS_NAME_MAX * 2]; snprintf(title, sizeof title, "%s", TR(titles[F.mode]));
    if (F.mode == LIST) { char labels[10][INK_FS_NAME_MAX]; size_t ends[10]; int n = crumbs(F.path, labels, ends, 10); snprintf(title, sizeof title, "%s", n ? labels[n - 1] : TR("文件")); }
    if (F.mode == NAME) snprintf(title, sizeof title, "%s", F.new_text ? TR("新建文本") : TR("新建文件夹"));
    if (F.mode == ACTIONS && F.selected.dir) snprintf(title, sizeof title, "%s", TR("这个文件夹"));
    if (F.mode == BATCH && F.bstate != B_ASK) snprintf(title, sizeof title, "%s", F.bstate == B_SPACE ? TR("空间不够") : F.bstate == B_DONE ? TR("结果") : F.copy ? TR("拷贝") : TR("移动"));
    F.model = (ink_ui_page_spec_t){.id = PAGES[F.mode].name, .title = F.mode == LIST ? TR("文件") : TR(titles[F.mode]), .object = {"files", F.epoch}, .fields = F.fields, .actions = F.actions};
    ink_shell_statusbar(screen, true);
    ink_shell_bind(ink_ui_circle(screen, "back", INK_MARGIN, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_STATIC, NULL, NULL), "files.up", NULL);
    int folder = F.mode == LIST && !F.multi && !root_dir(F.path) ? ink_ui_text_width(TR("这个文件夹"), INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP : 0;
    int top = INK_STATUS_BAR + INK_PRIMARY, bottom = INK_CANVAS_H - INK_MARGIN, w = INK_CANVAS_W - 2 * INK_MARGIN;
    ink_ui_line(screen, title, INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN + INK_CONTROL + INK_GAP, INK_STATUS_BAR + INK_GAP_S + (INK_CONTROL - ink_ui_line_height(INK_FONT_TITLE)) / 2, w - INK_CONTROL - INK_GAP - folder);
    F.actions[0] = (ink_ui_action_spec_t){.id = "prev", .label = TR("上一页"), .capability = "files.prev"};
    F.actions[1] = (ink_ui_action_spec_t){.id = "next", .label = TR("下一页"), .capability = "files.next"}; F.model.action_count = 2;
    if (F.mode == NAME) {
        F.busy = false; static const char *const kinds[] = {TR_NOOP("文件夹"), TR_NOOP("文本")}; const char *labels[] = {TR(kinds[0]), TR(kinds[1])};
        lv_obj_t *seg = ink_ui_segments(screen, labels, 2, F.new_text, INK_MARGIN, top + INK_GAP_S, w, NULL, NULL);
        if (seg) { ink_shell_bind(lv_obj_get_child(seg, 0), "files.new", "{\"text\":false}"); ink_shell_bind(lv_obj_get_child(seg, 1), "files.new", "{\"text\":true}"); }
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .initial = F.new_text ? TR("新建文本.txt") : "", .max = INK_FS_NAME_MAX - 1};
        F.field = ink_textfield_create(screen, &cfg, NULL, NULL, false); lv_obj_set_y(F.field, ink_ui_px(top + INK_SEGMENT + INK_GAP));
        ink_textfield_style(F.field, INK_FIELD_SINGLE, TR("名称"));
        F.notice = ink_ui_notice(screen, "", top + INK_SEGMENT + INK_GAP + ink_textfield_height(INK_FIELD_SINGLE) + INK_GAP_S);
        F.fields[0] = (ink_ui_field_spec_t){.id = "name", .label = TR("名称"), .role = "textbox"}; F.model.field_count = 1;
        F.actions[0] = (ink_ui_action_spec_t){.id = "name", .label = TR("名称"), .capability = "files.name", .enabled = true}; ink_textfield_bind_short(F.field, &F.model, &F.fields[0], &F.actions[0]);
        lv_obj_add_event_cb(F.field, renamed, LV_EVENT_VALUE_CHANGED, NULL); button(1, screen, F.new_text ? TR("新建文本") : TR("新建文件夹"), F.new_text ? "file.write" : "file.mkdir", F.rename, INK_MARGIN, bottom - INK_CONTROL, w, true);
        F.model.action_count = 2; renamed(NULL); return;
    }
    if (F.mode == EDIT) { F.notice = ink_ui_notice(screen, "", bottom - INK_CONTROL - INK_GAP - ink_ui_line_height(INK_FONT_NOTE)); F.body[0] = 0; F.busy = true; ink_args_t a; ink_args_init(&a); ink_args_str(&a, "path", F.edit_path); request("file.read", &a, edit_read, 0); return; }
    if (F.mode == ACTIONS) {
        if (F.selected.dir || F.known) { F.known = false; menu_page(F.selected.type == INK_FS_IMAGE ? "image" : NULL); return; } /* from the list: one refresh */
        memset(&F.selected, 0, sizeof F.selected); F.busy = true; ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", F.selected_id);
        F.notice = ink_ui_notice(screen, "", top + INK_GAP_S); status(TR("正在读取文件")); request("file.lookup", &a, selected, 0); return;
    }
    if (F.mode == BATCH) { batch_page(); return; }
    if (F.mode == SORT) {
        static const char *const by[] = {TR_NOOP("名字"), TR_NOOP("时间"), TR_NOOP("大小"), TR_NOOP("类型")}, *const order[] = {TR_NOOP("正序"), TR_NOOP("倒序")};
        const char *l1[] = {TR(by[0]), TR(by[1]), TR(by[2]), TR(by[3])}, *l2[] = {TR(order[0]), TR(order[1])};
        lv_obj_t *a = ink_ui_segments(screen, l1, 4, F.sort, INK_MARGIN, top + INK_GAP, w, NULL, NULL), *b = ink_ui_segments(screen, l2, 2, F.desc, INK_MARGIN, top + 2 * INK_GAP + INK_SEGMENT, w, NULL, NULL);
        for (int i = 0; i < 4; i++) { snprintf(F.args[i % 4], sizeof F.args[0], "{\"by\":\"%s\",\"desc\":%s}", SORTS[i], F.desc ? "true" : "false"); if (a) ink_shell_bind(lv_obj_get_child(a, i), "files.sort", F.args[i]);
            F.actions[i] = (ink_ui_action_spec_t){.id = SORTS[i], .label = l1[i], .capability = "files.sort", .args_json = F.args[i], .enabled = true}; }
        for (int i = 0; i < 2; i++) { snprintf(F.crumbs[i], sizeof F.crumbs[0], "{\"by\":\"%s\",\"desc\":%s}", SORTS[F.sort], i ? "true" : "false"); if (b) ink_shell_bind(lv_obj_get_child(b, i), "files.sort", F.crumbs[i]);
            F.actions[4 + i] = (ink_ui_action_spec_t){.id = i ? "desc" : "asc", .label = l2[i], .capability = "files.sort", .args_json = F.crumbs[i], .enabled = true}; }
        F.notice = ink_ui_notice(screen, TR("文件夹总在文件前面；名字里的数字按大小排"), top + 3 * INK_GAP + 2 * INK_SEGMENT); F.model.action_count = 6; return;
    }
    if (F.mode == SEARCH) {
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .initial = F.query, .max = sizeof F.query - 1};
        F.field = ink_textfield_create(screen, &cfg, NULL, NULL, false); lv_obj_set_y(F.field, ink_ui_px(top + INK_GAP_S)); ink_textfield_style(F.field, INK_FIELD_SEARCH, TR("文件名"));
        F.fields[0] = (ink_ui_field_spec_t){.id = "query", .label = TR("文件名"), .role = "textbox", .value = F.query}; F.model.field_count = 1;
        F.actions[2] = (ink_ui_action_spec_t){.id = "query", .label = TR("文件名"), .capability = "files.name", .enabled = true};
        ink_textfield_bind_short(F.field, &F.model, &F.fields[0], &F.actions[2]); lv_obj_add_event_cb(F.field, searched, LV_EVENT_VALUE_CHANGED, NULL); searched(NULL);
        button(3, screen, TR("搜索"), "files.search", F.target, INK_MARGIN, bottom - INK_CONTROL, w, true); F.model.action_count = 4;
        snprintf(F.empty, sizeof F.empty, F.query[0] ? TR("没找到名字里有“%s”的文件") : TR("在这个文件夹和里面的文件夹里找"), F.query);
        list_area(top + INK_GAP_S + ink_textfield_height(INK_FIELD_SEARCH) + INK_GAP, bottom - INK_CONTROL, false); shown(); return;
    }
    snprintf(F.empty, sizeof F.empty, "%s", TR("这里还没有文件"));
    if (F.mode == HOME) {
        home_items(); bool adult = ink_member_current()->role == INK_ROLE_ADULT;
        if (adult) { button(2, screen, TR("从手机或电脑传入"), "transfer.open", NULL, INK_MARGIN, bottom - 2 * INK_CONTROL - INK_GAP_S, w, false); button(3, screen, TR("连电脑"), "transfer.usb", NULL, INK_MARGIN, bottom - INK_CONTROL, w, false); }
        list_area(top + INK_GAP_S, adult ? bottom - 2 * INK_CONTROL - INK_GAP_S : bottom, false); shown();
        F.model.action_count = adult ? 4 : 2; ink_exec_post(INK_EXEC_BG, &F.job, rescan_bg, (void *)(uintptr_t)F.serial); return;
    }
    if (F.mode == MOVE) {
        char subject[192], where[224];
        if (F.multi) snprintf(subject, sizeof subject, F.copy ? TR("拷贝 %zu 个文件") : TR("移动 %zu 个文件"), F.marked_count);
        else snprintf(subject, sizeof subject, F.copy ? TR("拷贝：%s") : TR("移动：%s"), F.selected.name);
        ink_ui_line(screen, subject, INK_FONT_NOTE, INK_WEIGHT_MEDIUM, INK_MARGIN, top + INK_GAP_S, w);
        char from[INK_FS_PATH_MAX]; snprintf(from, sizeof from, "%s", F.multi ? F.path : F.selected.path); if (!F.multi) *strrchr(from, '/') = 0;
        bool same = F.dest[0] && !strcmp(F.dest, from) && !F.copy, inside = F.selected.dir && !F.multi && under(F.dest, F.selected.path);
        location(F.where, sizeof F.where, F.dest, strlen(F.dest)); snprintf(where, sizeof where, TR("放到：%s"), F.where);
        ink_ui_line(screen, same ? TR("已经在这个文件夹里") : F.dest[0] ? where : TR("先选一个文件夹"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, top + INK_GAP_S + ink_ui_line_height(INK_FONT_NOTE), w);
        int cw = (w - INK_GAP_S) / 2;
        button(2, screen, TR("新建文件夹"), "files.new", "{\"text\":false}", INK_MARGIN, bottom - INK_CONTROL, cw, false); enable(2, F.dest[0] && writable(F.dest));
        button(3, screen, F.copy ? TR("拷贝到这里") : TR("放到这里"), "files.place", NULL, INK_MARGIN + cw + INK_GAP_S, bottom - INK_CONTROL, cw, true); enable(3, F.dest[0] && writable(F.dest) && !same && !inside);
        F.model.action_count = 4;
        if (!F.dest[0]) { home_items(); list_area(top + INK_GAP_S + 2 * ink_ui_line_height(INK_FONT_NOTE) + INK_GAP, bottom - INK_CONTROL, false); shown(); return; }
        list_area(top + INK_GAP_S + 2 * ink_ui_line_height(INK_FONT_NOTE) + INK_GAP, bottom - INK_CONTROL, true); return;
    }
    /* LIST */
    if (F.multi) { F.notice = ink_ui_notice(screen, "", top + (INK_CONTROL - ink_ui_line_height(INK_FONT_NOTE)) / 2); selection_args(); batch_args(false); }
    else crumb_row(top);
    bool can = writable(F.path);
    const char *normal[][3] = {{TR("多选"), "files.multi", NULL}, {TR("搜索"), "files.search", NULL}, {TR("排序"), "files.sort", NULL}, {TR("新建"), "files.new", "{\"text\":false}"}};
    const char *multi[][3] = {{TR("取消"), "files.multi", NULL}, {TR("全选"), "files.all", NULL}, {TR("拷贝"), "files.move", "{\"copy\":true}"}, {TR("移动"), "files.move", NULL}, {TR("删除"), "file.delete_many", F.batch}};
    int y = F.multi ? toolbar(2, can ? 5 : 3, multi) : toolbar(2, can ? 4 : 3, normal);
    if (folder) { button((unsigned)F.model.action_count, screen, TR("这个文件夹"), "files.folder", NULL, INK_CANVAS_W - INK_MARGIN - folder, INK_STATUS_BAR + INK_GAP_S, folder, false); F.model.action_count++; }
    if (F.multi) for (unsigned i = 4; i < (can ? 7u : 5u); i++) enable(i, F.marked_count > 0);
    if (!F.stale && F.loaded[0] && !strcmp(F.loaded, F.path)) { list_area(top + INK_CONTROL + INK_GAP_S, y, false); F.busy = false; shown(); } /* sorted, back from a page: one refresh */
    else list_area(top + INK_CONTROL + INK_GAP_S, y, true);
    if (F.multi) status("");
}
static void destroy(void) { F.serial++; F.list = NULL; F.field = F.notice = F.preview = F.progress = NULL; memset(F.images, 0, sizeof F.images); memset(F.slots, 0, sizeof F.slots); }

/* Answers for the Flipper code (files_flipper.h). */
void files_fl_show_menu(bool show) {
    if (show) { F.result[0] = 0; ink_ui_push(&PAGES[ACTIONS], NULL); }
    else if (F.mode == ACTIONS && !F.previewing && !strcmp(ink_ui_page_name(), PAGES[ACTIONS].name)) ink_ui_pop();
    F.previewing = false;
}
static int depth(const char *path) { int n = 0; for (; *path; path++) n += *path == '/'; return n < 10 ? n : 9; }
void files_fl_enter(const char *path) {
    if (F.mode == LIST && F.path[0]) F.depth_pos[depth(F.path)] = F.position;
    F.result[0] = 0; F.marked_count = 0; F.all_marked = false; F.multi = F.multi && F.mode == LIST && !strcmp(path, F.path);
    snprintf(F.path, sizeof F.path, "%s", path); memset(&F.position, 0, sizeof F.position);
    if (F.mode == HOME) ink_ui_push(&PAGES[LIST], NULL); else ink_ui_redraw();
}
void files_fl_leave(void) {
    char *end = strrchr(F.path, '/');
    if (F.mode != LIST || !end || root_dir(F.path)) { ink_ui_pop(); return; }
    snprintf(F.back_from, sizeof F.back_from, "%s", F.path); *end = 0; F.position = F.depth_pos[depth(F.path)]; F.found = 0; F.multi = false; F.marked_count = 0; ink_ui_redraw();
}
bool files_fl_home(void) { return F.mode != LIST || root_dir(F.path) || !F.path[0]; }
void files_fl_reload(void) { F.stale = true; if (F.mode == LIST || F.mode == HOME) ink_ui_redraw(); }
void files_fl_turn(int dir) { F.serial++; if (F.list) ink_ui_list_turn(F.list, dir); }
void files_fl_removed(void) {
    char parent[INK_FS_PATH_MAX]; snprintf(parent, sizeof parent, "%.*s", (int)(strrchr(F.selected.path, '/') ? strrchr(F.selected.path, '/') - F.selected.path : 0), F.selected.path);
    F.stale = true; F.deleting[0] = 0;
    if (F.selected.dir && parent[0] && under(F.path, F.selected.path)) { memcpy(F.path, parent, sizeof F.path); F.position = F.depth_pos[depth(F.path)]; }
    if (F.mode == ACTIONS) ink_ui_pop(); else if (F.mode == HOME || F.mode == LIST) ink_ui_redraw();
}
void files_fl_open(const char *app) {
    ink_args_t a; ink_args_init(&a);
    if (strchr(app, '/')) { status(TR("这种文件暂时打不开")); F.previewing = true; return; }
    F.previewing = !strcmp(app, "files.preview");
    if (F.previewing) { ink_args_str(&a, "path", F.selected.path); ink_args_int(&a, "max_bytes", sizeof F.preview_text - 1); request("file.read", &a, opened, 0); return; }
    if (!strcmp(app, "photo.open")) ink_args_int(&a, "id", F.selected_id); else ink_args_str(&a, "path", F.selected.path);
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .member = F.member, .reply_exec = INK_EXEC_UI, .done = operation_done, .ctx = (void *)((uintptr_t)F.serial << 3)};
    if (ink_cap_call(app, &a, &opts)) status(TR("没能打开，请重试"));
}
/* The removal itself is told by file.changed (a confirmation comes first): see changed(). */
static void deleted(void *ctx, const ink_receipt_t *rc) { if (current(ctx)) status(rc->status == INK_RC_PENDING_CONFIRMATION ? TR("请在设备上确认") : rc->say[0] ? rc->say : ""); }
void files_fl_scene(int scene) {
    if (scene == FILES_SCENE_RENAME) { if (F.field) ink_textfield_short_focus(F.field); return; }
    ink_args_t a; ink_args_init(&a); ink_args_str(&a, "path", F.selected.path); memcpy(F.deleting, F.selected.path, sizeof F.deleting);
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .member = F.member, .reply_exec = INK_EXEC_UI, .done = deleted, .ctx = (void *)((uintptr_t)F.serial << 3)};
    if (scene == FILES_SCENE_DELETE && ink_cap_call("file.delete", &a, &opts)) status(TR("没能完成，请重试"));
}
void files_fl_stop(void) { ink_ui_pop(); }

static bool moved_to(const char *json, char *to) {
    ink_json_t *j = ink_app_alloc(sizeof *j, true); to[0] = 0;
    if (j && !ink_json_parse(j, json, strlen(json))) ink_json_str(j, ink_json_find(j, 0, "to"), to, INK_FS_PATH_MAX);
    ink_app_free(j); return to[0];
}
static void operation_done(void *ctx, const ink_receipt_t *rc) {
    if (!current(ctx)) return;
    F.busy = false;
    if (rc->status == INK_RC_DONE && (F.mode == NAME || F.mode == EDIT)) { if (F.mode == EDIT) { ink_db_t *db = ink_db_open_cached_member(&DRAFT_REFS, F.member); if (db) { ink_db_del(db, F.ref, INK_SRC_TOUCH); ink_db_flush(db); ink_db_close(db); } snprintf(F.result, sizeof F.result, "%s", TR("文件已更新")); } ink_ui_pop(); return; }
    if (F.mode == EDIT && F.field && rc->status != INK_RC_DONE) {
        lv_area_t keyboard; ink_shell_keyboard_area(&keyboard); status(TR("文件没有更新，文字仍在这里"));
        lv_obj_set_width(F.notice, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_CONTROL - INK_CIRCLE_SMALL - 2 * INK_GAP_S));
        lv_obj_set_y(F.notice, keyboard.y1 - ink_ui_px(2 * ink_ui_line_height(INK_FONT_NOTE) + INK_GAP));
        lv_obj_set_style_bg_color(F.notice, ink_ui_paper(), 0); lv_obj_set_style_bg_opa(F.notice, LV_OPA_COVER, 0); lv_obj_move_foreground(F.notice); return;
    }
    char to[INK_FS_PATH_MAX];
    if (F.mode == MOVE && F.selected.dir && !F.multi && (rc->status == INK_RC_DONE || rc->status == INK_RC_QUEUED)) { /* a folder copied or moved */
        if (moved_to(F.target, to) && !F.copy && !strcmp(F.selected.path, F.path)) memcpy(F.path, to, sizeof F.path);
        F.stale = true; F.position = F.depth_pos[depth(F.path)]; pop_to(LIST); return;
    }
    if (rc->status == INK_RC_DONE && F.mode == ACTIONS && moved_to(F.rename, to) && !strncmp(rc->object, "file:", 5) && !strncmp(rc->object + 5, to, 40)) {
        if (F.selected.dir) { if (!strcmp(F.selected.path, F.path)) memcpy(F.path, to, sizeof F.path); F.stale = true; ink_ui_pop(); return; }
    }
    if (rc->status == INK_RC_PARTIAL) { status(F.result); return; }
    if (strstr(rc->detail, "exists")) { status(TR("这个名字已存在，请换一个")); return; }
    status(rc->say[0] ? rc->say : rc->status == INK_RC_DONE ? TR("已完成") : rc->status == INK_RC_PENDING_CONFIRMATION ? TR("请在设备上确认") : TR("没能完成，请检查文件后重试"));
}
static bool item_at(const char *path, item_t *out) { for (size_t i = 0; i < F.count; i++) if (!strcmp(F.items[i].path, path)) { *out = F.items[i]; return true; } return false; }
static void navigate(ink_call_t *c) {
    if (!ink_ui_page_current() || F.epoch != ink_member_epoch() || strcmp(ink_ui_action_page(), PAGES[F.mode].name)) { c->rc.status = INK_RC_STALE; return; }
    const char *id = c->cap->id; char path[INK_FS_PATH_MAX] = "", text[64] = ""; int64_t value = 0; bool flag = false;
    if (!strcmp(id, "files.name")) { if (!F.field || (F.mode != SEARCH && !writable(F.mode == NAME ? F.name_dir : F.selected.path))) c->rc.status = INK_RC_REJECTED; else ink_textfield_short_focus(F.field); return; }
    if (F.busy && strcmp(id, "files.up")) { c->rc.status = INK_RC_FAILED; return; }
    if (!strcmp(id, "files.new")) {
        const char *dir = F.mode == MOVE ? F.dest : F.mode == NAME ? F.name_dir : F.path;
        if ((F.mode != LIST && F.mode != MOVE && F.mode != NAME) || !dir[0] || !writable(dir)) { c->rc.status = INK_RC_REJECTED; return; }
        F.new_text = false; ink_args_get_bool(&c->args, "text", &F.new_text); F.new_text &= F.mode != MOVE; snprintf(F.name_dir, sizeof F.name_dir, "%s", dir);
        if (F.mode == NAME) ink_ui_redraw(); else ink_ui_push(&PAGES[NAME], NULL); return;
    }
    if (!strcmp(id, "files.edit")) { const char *ext = strrchr(F.selected.path, '.'); if (F.mode != ACTIONS || !ext || strcmp(ext, ".txt") || !writable(F.selected.path)) { c->rc.status = INK_RC_REJECTED; return; } memcpy(F.edit_path, F.selected.path, sizeof F.edit_path); ink_ui_push(&PAGES[EDIT], NULL); return; }
    if (!strcmp(id, "files.multi") || !strcmp(id, "files.mark") || !strcmp(id, "files.all")) {
        if (F.mode != LIST) { c->rc.status = INK_RC_INVALID; return; }
        if (!strcmp(id, "files.multi")) { F.multi = !F.multi; F.marked_count = 0; F.result[0] = 0; ink_ui_redraw(); return; }
        if (!F.multi) { c->rc.status = INK_RC_INVALID; return; }
        bool was = F.marked_count > 0; int at = -1;
        if (!strcmp(id, "files.all")) { F.marked_count = 0; for (size_t i = 0; i < F.rows; i++) if (!F.items[F.view[i]].dir) F.marked[F.marked_count++] = F.items[F.view[i]].id; }
        else { ink_args_get_int(&c->args, "id", &value); size_t i = 0; while (i < F.marked_count && F.marked[i] != (uint32_t)value) i++;
            if (i < F.marked_count) F.marked[i] = F.marked[--F.marked_count]; else for (i = 0; i < F.rows; i++) if (F.items[F.view[i]].id == (uint32_t)value && !F.items[F.view[i]].dir) { F.marked[F.marked_count++] = (uint32_t)value; break; }
            for (size_t r = F.position.page * F.capacity; r < F.rows && r < (F.position.page + 1) * F.capacity; r++) if (F.items[F.view[r]].id == (uint32_t)value) at = (int)(r % F.capacity);
        }
        selection_args(); batch_args(false);
        if (at < 0 || !F.slots[at % 4]) { ink_ui_list_refresh(F.list, F.rows); status(""); for (unsigned i = 4; i < 7; i++) if (F.buttons[i]) enable(i, F.marked_count > 0); return; }
        /* one row and the count: nothing else on the page changed (D-127) */
        lv_obj_t *slot = F.slots[at % 4], *check = lv_obj_get_child(slot, 7); bool on = is_marked((uint32_t)value);
        if (on) lv_obj_remove_flag(check, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(check, LV_OBJ_FLAG_HIDDEN);
        snprintf(F.marks[at % 4], sizeof F.marks[0], "%s", on ? TR("已选择") : TR("未选择")); ink_ui_hint_obj(slot, INK_REFRESH_UI); status("");
        if (was != (F.marked_count > 0)) for (unsigned i = 4; i < 7; i++) if (F.buttons[i]) enable(i, F.marked_count > 0);
        return;
    }
    if (!strcmp(id, "files.next") || !strcmp(id, "files.prev")) { if (!F.list) { c->rc.status = INK_RC_INVALID; return; } int pg = (int)F.position.page; files_fl_event(!strcmp(id, "files.next") ? FILES_EV_NEXT : FILES_EV_PREV); if ((int)F.position.page == pg) c->rc.status = INK_RC_INVALID; return; }
    if (!strcmp(id, "files.browse")) {
        ink_args_get_str(&c->args, "path", path, sizeof path); ink_args_get_int(&c->args, "find", &value);
        bool allowed = under(path, "/me") || under(path, "/family") || (!strncmp(path, "/members/", 9) && path[9] && ink_member_current()->role == INK_ROLE_ADULT);
        if (!allowed || (F.mode == MOVE && !writable(path)) || (F.mode != HOME && F.mode != LIST && F.mode != MOVE && F.mode != SEARCH)) { c->rc.status = INK_RC_REJECTED; return; }
        F.found = (uint32_t)value; F.result[0] = 0;
        if (F.mode == MOVE) { snprintf(F.dest, sizeof F.dest, "%s", path); memset(&F.position, 0, sizeof F.position); ink_ui_redraw(); return; }
        if (F.mode == SEARCH) { snprintf(F.path, sizeof F.path, "%s", path); F.stale = true; memset(&F.position, 0, sizeof F.position); F.multi = false; ink_ui_pop(); return; }
        if (!strcmp(path, F.path) && F.mode == LIST) { F.stale = true; ink_ui_redraw(); return; }
        files_fl_select(path, true, false); files_fl_event(FILES_EV_ENTER); return;
    }
    if (!strcmp(id, "files.select")) {
        ink_args_get_int(&c->args, "id", &value); ink_args_get_str(&c->args, "path", path, sizeof path);
        if (!value && path[0]) { /* a favourite file: its folder first, then its menu */
            if (F.mode != HOME || !strrchr(path, '/') || !(under(path, "/family") || under(path, "/me") || (ink_member_current()->role == INK_ROLE_ADULT && !strncmp(path, "/members/", 9)))) { c->rc.status = INK_RC_REJECTED; return; }
            snprintf(F.path, sizeof F.path, "%.*s", (int)(strrchr(path, '/') - path), path); memset(&F.position, 0, sizeof F.position); memset(&F.selected, 0, sizeof F.selected);
            snprintf(F.selected.path, sizeof F.selected.path, "%s", path); F.found = F.selected_id = 0; F.open_found = F.from_fav = F.stale = true; ink_ui_push(&PAGES[LIST], NULL); return;
        }
        F.selected_id = (uint32_t)value; F.from_fav = false; memset(&F.selected, 0, sizeof F.selected);
        for (size_t i = 0; i < F.count; i++) if (F.items[i].id == F.selected_id) { F.selected = F.items[i]; F.known = true; }
        files_fl_select(F.selected.path, false, false); files_fl_event(FILES_EV_MENU); return;
    }
    if (!strcmp(id, "files.folder")) {
        if (F.mode != LIST || root_dir(F.path) || !F.path[0]) { c->rc.status = INK_RC_REJECTED; return; }
        memset(&F.selected, 0, sizeof F.selected); snprintf(F.selected.path, sizeof F.selected.path, "%s", F.path); snprintf(F.selected.name, sizeof F.selected.name, "%s", base(F.path));
        F.selected.dir = true; F.selected_id = 0; F.from_fav = false; F.result[0] = 0; ink_ui_push(&PAGES[ACTIONS], NULL); return;
    }
    if (!strcmp(id, "files.fav")) {
        ink_args_get_str(&c->args, "path", path, sizeof path); item_t it = {0};
        bool dir = !strcmp(path, F.selected.path) ? F.selected.dir : !strcmp(path, F.path) || (item_at(path, &it) && it.dir);
        if (!(under(path, "/me") || under(path, "/family") || (ink_member_current()->role == INK_ROLE_ADULT && !strncmp(path, "/members/", 9))) || root_dir(path)) { c->rc.status = INK_RC_REJECTED; return; }
        if (dir) { files_fl_pin_folder(path); if (F.mode == ACTIONS) ink_ui_pop(); return; }
        files_fl_select(path, false, F.from_fav && F.mode == ACTIONS); files_fl_event(FILES_EV_PIN); return;
    }
    if (!strcmp(id, "files.sort")) {
        if (!ink_args_find(&c->args, "by")) { if (F.mode != LIST) c->rc.status = INK_RC_INVALID; else ink_ui_push(&PAGES[SORT], NULL); return; }
        ink_args_get_str(&c->args, "by", text, sizeof text); ink_args_get_bool(&c->args, "desc", &flag); int by = 0; while (by < 4 && strcmp(text, SORTS[by])) by++;
        if (by == 4 || (F.mode != SORT && F.mode != LIST)) { c->rc.status = INK_RC_INVALID; return; }
        files_prefs_t *p = files_prefs_begin(); p->sort = F.sort = by; p->desc = F.desc = flag; files_prefs_end(true);
        if (F.mode == SORT) ink_ui_pop(); else { build_view(); if (F.list) ink_ui_list_refresh(F.list, F.rows); }
        return;
    }
    if (!strcmp(id, "files.search")) {
        if (F.mode != LIST && F.mode != SEARCH) { c->rc.status = INK_RC_INVALID; return; }
        if (ink_args_get_str(&c->args, "query", F.query, sizeof F.query) < 0) F.query[0] = 0;
        F.found = 0; if (F.mode == SEARCH) memset(&F.position, 0, sizeof F.position);
        if (F.mode == LIST) { F.depth_pos[depth(F.path)] = F.position; memset(&F.position, 0, sizeof F.position); ink_ui_push(&PAGES[SEARCH], NULL); } else ink_ui_redraw();
        if (F.query[0] && !query_ok()) snprintf(c->rc.say, sizeof c->rc.say, "%s", TR("至少输入 2 个字母或数字，或 1 个汉字"));
        return;
    }
    if (!strcmp(id, "files.move")) {
        ink_args_get_bool(&c->args, "copy", &flag);
        if (!((F.mode == LIST && F.multi && F.marked_count && (flag || writable(F.path))) || (F.mode == ACTIONS && (F.selected.id || F.selected.dir) && (flag || writable(F.selected.path))))) { c->rc.status = INK_RC_REJECTED; return; }
        F.copy = flag; F.dest[0] = 0; F.multi = F.multi && F.mode == LIST;
        files_prefs_t *p = files_prefs_begin(); if (p->last[0] && writable(p->last)) snprintf(F.dest, sizeof F.dest, "%s", p->last); files_prefs_end(false);
        if (F.path[0]) F.depth_pos[depth(F.path)] = F.position;
        memset(&F.position, 0, sizeof F.position); ink_ui_push(&PAGES[MOVE], NULL); return;
    }
    if (!strcmp(id, "files.place")) {
        char from[INK_FS_PATH_MAX]; snprintf(from, sizeof from, "%s", F.multi ? F.path : F.selected.path); if (!F.multi) *strrchr(from, '/') = 0;
        if (F.mode != MOVE || !F.dest[0] || !writable(F.dest) || (!strcmp(F.dest, from) && !F.copy) || (F.selected.dir && !F.multi && under(F.dest, F.selected.path))) { c->rc.status = INK_RC_REJECTED; return; }
        if (F.selected.dir && !F.multi) { /* folders: no ids, a single copy or move */
            char to[INK_FS_PATH_MAX]; if (!join(to, sizeof to, F.dest, F.selected.name)) { c->rc.status = INK_RC_INVALID; return; }
            pair_json(F.target, sizeof F.target, "from", F.selected.path, "to", to); files_prefs_t *p = files_prefs_begin(); snprintf(p->last, sizeof p->last, "%s", F.dest); files_prefs_end(true);
            ink_call_opts_t opts = {.src = c->src, .member = F.member, .args_json = F.target, .reply_exec = INK_EXEC_UI, .done = operation_done, .ctx = (void *)((uintptr_t)F.serial << 3)};
            status(TR("正在处理")); if (ink_cap_call(F.copy ? "file.copy" : "file.move", NULL, &opts)) c->rc.status = INK_RC_FAILED; return;
        }
        F.bstate = B_CHECK; F.result[0] = 0; F.total = 0; ink_ui_push(&PAGES[BATCH], NULL); return;
    }
    if (!strcmp(id, "files.start")) { if (F.mode != BATCH || F.bstate != B_SPACE) { c->rc.status = INK_RC_INVALID; return; } F.bstate = F.conf_n ? B_ASK : B_RUN; if (F.bstate == B_RUN) batch_start(); else ink_ui_redraw(); return; }
    if (!strcmp(id, "files.conflict")) {
        if (F.mode != BATCH || F.bstate != B_ASK) { c->rc.status = INK_RC_INVALID; return; }
        if (ink_args_get_bool(&c->args, "all", &flag)) F.all = flag;
        if (ink_args_get_str(&c->args, "choice", text, sizeof text) < 0) { ink_ui_redraw(); return; }
        int ch = 0; while (ch < 3 && strcmp(text, CHOICES[ch])) ch++;
        char from[INK_FS_PATH_MAX]; snprintf(from, sizeof from, "%s", F.multi ? F.path : F.selected.path); if (!F.multi) *strrchr(from, '/') = 0;
        if (ch == 3 || (ch == 2 && F.copy && !strcmp(F.dest, from))) { c->rc.status = INK_RC_INVALID; return; }
        for (size_t i = F.conf_at; i < (F.all ? F.conf_n : F.conf_at + 1); i++) F.conf[i].choice = (uint8_t)ch;
        F.conf_at = F.all ? F.conf_n : F.conf_at + 1;
        if (F.conf_at < F.conf_n) ink_ui_redraw(); else batch_start();
        return;
    }
    if (!strcmp(id, "files.preview")) {
        ink_args_get_int(&c->args, "id", &value);
        if (F.mode != ACTIONS || value != F.selected_id || !F.selected.id) { c->rc.status = INK_RC_INVALID; return; }
        F.previewing = true; files_fl_open("files.preview"); return;
    }
    /* files.up */
    if (F.mode == BATCH) { F.stale = F.stale || !F.copy; F.position = F.depth_pos[depth(F.path)]; pop_to(LIST); return; }
    if (F.mode == MOVE && F.dest[0]) { char *end = strrchr(F.dest, '/'); if (end && end != F.dest && !root_dir(F.dest)) *end = 0; else F.dest[0] = 0; memset(&F.position, 0, sizeof F.position); ink_ui_redraw(); return; }
    if (F.mode == LIST || F.mode == HOME) { files_fl_event(FILES_EV_EXIT); return; }
    if ((F.mode == MOVE || F.mode == SEARCH) && F.path[0]) F.position = F.depth_pos[depth(F.path)];
    ink_ui_pop();
}
static int snapshot(char *out, size_t cap) {
    int n = F.mode == EDIT && F.field ? ink_textfield_long_snapshot(F.field, out, cap) : F.list ? ink_ui_list_snapshot(F.list, out, cap) : ink_ui_semantics_snapshot(&F.model, out, cap);
    if (n < 0) return -1;
    ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)n - 1}; ink_jw_raw(&w, ",\"status\":"); ink_jw_str(&w, F.notice ? lv_label_get_text(F.notice) : "");
    location(F.where, sizeof F.where, directory(), strlen(directory()));
    ink_jw_raw(&w, ",\"directory\":"); ink_jw_str(&w, directory()); ink_jw_raw(&w, ",\"location\":"); ink_jw_str(&w, F.where); ink_jw_raw(&w, ",\"back\":\"files.up\"");
    if (F.mode == LIST || F.mode == SORT) { ink_jw_raw(&w, ",\"sort\":{\"by\":"); ink_jw_str(&w, SORTS[F.sort]); ink_jw_raw(&w, F.desc ? ",\"desc\":true}" : ",\"desc\":false}"); }
    if (F.mode == SEARCH) { ink_jw_raw(&w, ",\"query\":"); ink_jw_str(&w, F.query); }
    ink_jw_raw(&w, ",\"selected_count\":"); ink_jw_int(&w, F.marked_count);
    if (F.mode == LIST || F.mode == ACTIONS || F.mode == BATCH) { bool first = true; ink_jw_raw(&w, F.mode == ACTIONS ? ",\"menu\":[" : ",\"toolbar\":[");
        for (size_t i = F.mode == BATCH ? 0 : 2; i < F.model.action_count && i < ACTION_MAX; i++) { const ink_ui_action_spec_t *a = &F.actions[i]; if (!a->enabled || !a->capability) continue;
            if (!first) ink_jw_raw(&w, ",");
            first = false; ink_jw_raw(&w, "{\"capability\":"); ink_jw_str(&w, a->capability); ink_jw_raw(&w, ",\"label\":"); ink_jw_str(&w, a->label); ink_jw_raw(&w, ",\"args_json\":"); ink_jw_str(&w, a->args_json ? a->args_json : "{}"); ink_jw_raw(&w, "}"); } ink_jw_raw(&w, "]"); }
    if (F.mode == BATCH && F.bstate == B_ASK) { ink_jw_raw(&w, ",\"conflict\":{\"at\":"); ink_jw_int(&w, (int64_t)F.conf_at + 1); ink_jw_raw(&w, ",\"of\":"); ink_jw_int(&w, (int64_t)(F.conf_total > F.conf_n ? F.conf_total : F.conf_n)); ink_jw_raw(&w, ",\"name\":"); ink_jw_str(&w, F.conf[F.conf_at].name); ink_jw_raw(&w, F.all ? ",\"all\":true}" : ",\"all\":false}"); }
    if (F.mode == BATCH && F.bstate == B_DONE) { ink_jw_raw(&w, ",\"result\":{\"succeeded\":"); ink_jw_int(&w, (int64_t)F.succeeded); ink_jw_raw(&w, ",\"skipped\":"); ink_jw_int(&w, (int64_t)F.skipped);
        ink_jw_raw(&w, ",\"failed\":"); ink_jw_int(&w, (int64_t)F.failed); ink_jw_raw(&w, ",\"not_done\":"); ink_jw_int(&w, (int64_t)F.not_done); ink_jw_raw(&w, ",\"renamed\":"); ink_jw_str(&w, F.renamed); ink_jw_raw(&w, "}"); }
    if (F.mode == EDIT && F.field) { ink_jw_raw(&w, ",\"save\":{\"capability\":\"file.write\",\"args\":"); ink_jw_raw(&w, F.target); ink_jw_raw(&w, "}"); }
    if (F.mode == HOME && ink_member_current()->role == INK_ROLE_ADULT) ink_jw_raw(&w, ",\"transfer_actions\":[{\"capability\":\"transfer.open\",\"label\":\"从手机或电脑传入\"},{\"capability\":\"transfer.usb\",\"label\":\"连电脑\"}]");
    ink_jw_raw(&w, "}");
    if (w.overflow) { out[0] = 0; return -1; }
    return (int)w.len;
}
static bool page_key(ink_key_t key, bool lng) { return F.list && ink_ui_list_key(F.list, key, lng); }
static void changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    char path[INK_FS_PATH_MAX], kind[16]; ink_args_get_str(args, "path", path, sizeof path); ink_args_get_str(args, "kind", kind, sizeof kind);
    if (F.deleting[0] && !strcmp(kind, "removed") && !strcmp(path, F.deleting) && F.epoch == ink_member_epoch()) { files_fl_deleted(path, true); return; } /* Flipper: removed, unpinned, off the list */
    if (F.renaming[0] && !strcmp(kind, "moved") && !strcmp(path, F.renaming)) { files_fl_renamed(F.renamed_from, F.renaming); F.renaming[0] = 0; } /* Flipper's rename scene: the favourite follows */
    char dir[INK_FS_PATH_MAX]; snprintf(dir, sizeof dir, "%s", path); char *end = strrchr(dir, '/'); if (end) *end = 0;
    if (F.loaded[0] && (under(path, F.loaded) || !strcmp(dir, F.loaded))) F.stale = true;
    if (!ink_ui_page_current() || F.epoch != ink_member_epoch() || strcmp(ink_ui_page_name(), PAGES[F.mode].name)) return;
    if (F.mode == EDIT || F.mode == NAME || F.mode == BATCH || F.mode == SORT || F.mode == SEARCH || F.multi || F.busy) return;
    if (F.mode == HOME && !strcmp(kind, "indexed")) return; /* Roots do not depend on the index; keep queued taps valid. */
    if (F.mode == LIST && !F.stale) return; /* another folder changed */
    if (F.mode == MOVE && strcmp(dir, F.dest)) return;
    if (F.mode == ACTIONS && !strcmp(kind, "removed") && !strcmp(path, F.selected.path)) files_fl_deleted(path, true);
    else if (F.mode != ACTIONS || !F.selected.dir) ink_ui_redraw();
}
/* The card came back, or the index is ready: a page that could not read retries (no polling, D-138). */
static void card_back(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (F.error && ink_ui_page_current() && F.epoch == ink_member_epoch() && !strcmp(ink_ui_page_name(), PAGES[F.mode].name) && (F.mode == LIST || F.mode == MOVE)) ink_ui_redraw();
}
static void index_state(void *ctx, int handle, const char *id) { (void)handle; (void)id; card_back(ctx, NULL, NULL); }
#define PAGE(name_) {.name = name_, .app = "files", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key}
static const ink_activity_t PAGES[] = {PAGE("Files"), PAGE("FileList"), PAGE("FileActions"), PAGE("FileMove"), PAGE("FileName"), PAGE("FileEdit"), PAGE("FileSort"), PAGE("FileSearch"), PAGE("FileBatch")};
_Static_assert(sizeof PAGES / sizeof *PAGES <= PAGE_COUNT, "file page count");
static const char *const PERMISSIONS[] = {"file", "media", NULL};
static const ink_app_def_t APP = {.id = "files", .title = TR_NOOP("文件"), .icon = INK_UI_ICON(folder), .entry = &PAGES[HOME], .system_app = true, .child_ok = true, .permissions = PERMISSIONS};
#define ID_INPUT "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}},\"required\":[\"id\"]}"
#define CAP(id_, title_, input_) {.id = id_, .app = "files", .title = title_, .input = input_, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}
static const ink_cap_t CAPS[] = {
    CAP("files.new", "新建文件夹或文本", "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"boolean\"}}}"),
    CAP("files.edit", "编辑当前 txt 文件", NULL),
    CAP("files.multi", "进入或退出文件多选", NULL),
    CAP("files.mark", "选择或取消选择文件（只刷这一行）", ID_INPUT),
    CAP("files.all", "选择当前文件夹的全部文件", NULL),
    CAP("files.browse", "浏览文件夹；find 给文件编号时翻到它所在的页并标出来", "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127},\"find\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}},\"required\":[\"path\"]}"),
    CAP("files.select", "打开文件菜单（按编号；收藏的文件给 path）", "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295},\"path\":{\"type\":\"string\",\"maxLength\":127}}}"),
    CAP("files.move", "选拷贝或移动的目标文件夹（从上次放的地方开始）", "{\"type\":\"object\",\"properties\":{\"copy\":{\"type\":\"boolean\"}}}"),
    CAP("files.name", "编辑文件名或搜索词", NULL),
    CAP("files.preview", "预览当前文本文件开头", ID_INPUT),
    CAP("files.next", "文件下一页", NULL),
    CAP("files.prev", "文件上一页", NULL),
    CAP("files.up", "返回文件上一级（结果页：完成）", NULL),
    CAP("files.search", "在当前文件夹和子文件夹里按名字找（不给 query 打开搜索页）", "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"maxLength\":63}}}"),
    CAP("files.sort", "排序：名字、时间、大小、类型，正序或倒序（不给 by 打开排序页）", "{\"type\":\"object\",\"properties\":{\"by\":{\"type\":\"string\",\"enum\":[\"name\",\"time\",\"size\",\"type\"]},\"desc\":{\"type\":\"boolean\"}}}"),
    CAP("files.fav", "收藏或取消收藏一个文件或文件夹", "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"maxLength\":127}},\"required\":[\"path\"]}"),
    CAP("files.folder", "打开“这个文件夹”的菜单：改名、移动、拷贝、删除、收藏", NULL),
    CAP("files.place", "放到这里：先查同名和空间，再拷贝或移动", NULL),
    CAP("files.start", "空间不够时仍开始，放得下的先做", NULL),
    CAP("files.conflict", "同名文件怎么办：保留两个、跳过或替换；all 表示后面的同名都这样", "{\"type\":\"object\",\"properties\":{\"choice\":{\"type\":\"string\",\"enum\":[\"keep_both\",\"skip\",\"replace\"]},\"all\":{\"type\":\"boolean\"}}}"),
};
int ink_transfer_pages_init(void);
int ink_files_init(void) {
    return ink_db_register(&DRAFT_REFS) || files_fl_init() || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_cap_subscribe("file.changed", changed, NULL, INK_EXEC_UI) < 0 ||
        ink_cap_subscribe("file.batch_progress", batch_progress, NULL, INK_EXEC_UI) < 0 || ink_cap_subscribe("usb.changed", card_back, NULL, INK_EXEC_UI) < 0 ||
        ink_state_watch("file.index_state", index_state, NULL, INK_EXEC_UI) || ink_transfer_pages_init() ? -1 : 0;
}
