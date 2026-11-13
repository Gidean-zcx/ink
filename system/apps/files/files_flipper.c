/* Glue for the copied Flipper archive code (vendor/flipper): FuriString, the favourites "files" kept in the
 * member's files.prefs record, the menu recorder and the browser helpers, which the file pages answer. */
#pragma GCC diagnostic ignored "-Wunused-function" /* archive_files.h defines its array helpers in the header */
#include <stdio.h>
#include <strings.h>
#include "ink_flipper.h"
#include "files_flipper.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
_Static_assert((int)FILES_EV_RUN == (int)ArchiveBrowserEventFileMenuRun && (int)FILES_EV_ENTER == (int)ArchiveBrowserEventEnterDir &&
    (int)FILES_EV_NEXT == (int)ArchiveBrowserEventLoadNextItems && (int)FILES_EV_EXIT == (int)ArchiveBrowserEventExit &&
    (int)FILES_TYPE_ZIP == (int)ArchiveFileTypeZip && (int)FILES_TYPE_UNKNOWN == (int)ArchiveFileTypeUnknown, "Flipper event and type numbers");
#define FAV_CAP sizeof(((files_prefs_t *)0)->fav)
struct FuriString { char s[2 * INK_FS_PATH_MAX]; size_t n; };
struct File { int which; size_t pos; FS_Error err; };
static struct {
    osal_mutex_t *lock; osal_task_t *owner; int depth; bool dirty, exists[2], has, dir, removed; uint32_t epoch, rec; char member[16];
    files_prefs_t prefs, saved; char tmp[FAV_CAP]; ArchiveApp app; ArchiveBrowserView view; ArchiveBrowserViewModel model; ArchiveFile_t file;
    int menu_n; const char *menu[FILES_MENU_MAX]; char json[INK_DB_RECORD_MAX];
    osal_task_t *scan_task; uint32_t scan_epoch; char lines[FAV_CAP], gone[FAV_CAP]; /* files_fl_rescan's card reads, made unlocked */
} G INK_PSRAM_BSS;

FuriString *furi_string_alloc(void) { return osal_calloc(1, sizeof(FuriString), OSAL_MEM_EXTERNAL); }
void furi_string_set_str(FuriString *s, const char *v) { if (v != s->s) snprintf(s->s, sizeof s->s, "%s", v); s->n = strlen(s->s); }
FuriString *furi_string_alloc_set_str(const char *v) { FuriString *s = furi_string_alloc(); furi_string_set_str(s, v); return s; }
FuriString *furi_string_alloc_vprintf(const char *format, va_list args) { FuriString *s = furi_string_alloc(); if (vsnprintf(s->s, sizeof s->s, format, args) < 0) s->s[0] = 0; s->n = strlen(s->s); return s; }
int furi_string_printf(FuriString *s, const char *format, ...) { va_list a; va_start(a, format); int n = vsnprintf(s->s, sizeof s->s, format, a); va_end(a); s->n = strlen(s->s); return n; }
void furi_string_free(FuriString *s) { osal_free(s); }
void furi_string_reset(FuriString *s) { s->s[s->n = 0] = 0; }
void furi_string_push_back(FuriString *s, char c) { if (s->n + 1 < sizeof s->s) { s->s[s->n++] = c; s->s[s->n] = 0; } }
size_t furi_string_size(const FuriString *s) { return s->n; }
const char *furi_string_get_cstr(const FuriString *s) { return s->s; }
size_t furi_string_search_str(const FuriString *s, const char *needle) { const char *p = strstr(s->s, needle); return p ? (size_t)(p - s->s) : FURI_STRING_FAILURE; }
size_t furi_string_search_char(const FuriString *s, char c, size_t start) { const char *p = start < s->n ? strchr(s->s + start, c) : NULL; return p ? (size_t)(p - s->s) : FURI_STRING_FAILURE; }
void furi_string_right(FuriString *s, size_t start) { if (start > s->n) start = s->n; memmove(s->s, s->s + start, s->n - start + 1); s->n -= start; }
void furi_string_left(FuriString *s, size_t n) { if (n < s->n) s->s[s->n = n] = 0; }
bool furi_string_end_withi(const FuriString *s, const char *e) {
    for (;;) {
        size_t n = strcspn(e, "|");
        if (n && n <= s->n && !strncasecmp(s->s + s->n - n, e, n)) return true;
        if (!e[n]) return false;
        e += n + 1;
    }
}

/* The two favourites "files" in memory, read when the member changes and written on the last close. The preferences
 * are one JSON record ("files_prefs") in the member's files.drafts: the data store has no collection slot to spare. */
static char *buffer(int which) { return which ? G.tmp : G.prefs.fav; }
static int which(const char *path) { return !strcmp(path, ARCHIVE_FAV_PATH) ? 0 : !strcmp(path, ARCHIVE_FAV_TEMP_PATH) ? 1 : -1; }
static void load(void) {
    memset(&G.prefs, 0, sizeof G.prefs); G.tmp[0] = 0; G.rec = 0; G.epoch = ink_member_epoch();
    snprintf(G.member, sizeof G.member, "%s", ink_member_current()->id);
    ink_db_t *db = ink_db_open_member(ink_db_find_def("files.drafts"), G.member); ink_db_cursor_t c = {0}; ink_rec_t rec;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t sort = 0;
    while (db && j && ink_db_next(db, &c, &rec, G.json, sizeof G.json - 1) == 1) {
        G.json[rec.len < sizeof G.json ? rec.len : sizeof G.json - 1] = 0;
        if (rec.len >= sizeof G.json || ink_json_parse(j, G.json, rec.len) || ink_json_find(j, 0, "files_prefs") < 0) continue;
        G.rec = rec.id; ink_json_int(j, ink_json_find(j, 0, "sort"), &sort); G.prefs.sort = (int32_t)sort; G.prefs.desc = ink_json_eq(j, ink_json_find(j, 0, "desc"), "true");
        ink_json_str(j, ink_json_find(j, 0, "last"), G.prefs.last, sizeof G.prefs.last); ink_json_str(j, ink_json_find(j, 0, "fav"), G.prefs.fav, FAV_CAP); break;
    }
    osal_free(j); ink_db_close(db); G.exists[0] = G.prefs.fav[0]; G.exists[1] = false; G.saved = G.prefs;
}
/* What is written: only what differs from the record (a rescan that kept every line rewrites the same text). */
static bool changed(void) {
    return G.prefs.sort != G.saved.sort || G.prefs.desc != G.saved.desc || strcmp(G.prefs.last, G.saved.last) || strcmp(G.prefs.fav, G.saved.fav);
}
void *furi_record_open(const char *name) {
    if (!G.lock) G.lock = osal_mutex_create(); /* first used on the UI task (a page reads the sort) before any rescan is posted */
    if (G.owner != osal_task_self()) { osal_mutex_lock(G.lock, OSAL_WAIT_FOREVER); G.owner = osal_task_self(); }
    if (!G.depth++ && G.epoch != ink_member_epoch()) load();
    return (void *)name;
}
void furi_record_close(const char *name) {
    (void)name;
    if (--G.depth) return;
    ink_db_t *db = G.dirty && G.epoch == ink_member_epoch() && changed() ? ink_db_open_member(ink_db_find_def("files.drafts"), G.member) : NULL;
    ink_jw_t w; ink_jw_init(&w, G.json, sizeof G.json); ink_jw_raw(&w, "{\"files_prefs\":1,\"sort\":"); ink_jw_int(&w, G.prefs.sort);
    ink_jw_raw(&w, G.prefs.desc ? ",\"desc\":true,\"last\":" : ",\"desc\":false,\"last\":"); ink_jw_str(&w, G.prefs.last); ink_jw_raw(&w, ",\"fav\":"); ink_jw_str(&w, G.prefs.fav); ink_jw_raw(&w, "}");
    ink_db_write_t put = {.id = G.rec, .data = G.json, .len = w.len};
    if (db && !w.overflow && !ink_db_put(db, &put, &G.rec)) { ink_db_flush(db); G.saved = G.prefs; ink_probe_count("files.prefs_written"); }
    ink_db_close(db); G.dirty = false; G.owner = NULL; osal_mutex_unlock(G.lock);
}
files_prefs_t *files_prefs_begin(void) { furi_record_open(RECORD_STORAGE); return &G.prefs; }
void files_prefs_end(bool changed) { G.dirty |= changed; furi_record_close(RECORD_STORAGE); }
File *storage_file_alloc(Storage *s) { (void)s; return osal_calloc(1, sizeof(File), OSAL_MEM_EXTERNAL); }
void storage_file_free(File *f) { osal_free(f); }
bool storage_file_open(File *f, const char *path, FS_AccessMode access, FS_OpenMode mode) {
    (void)access; f->which = which(path); f->pos = 0;
    f->err = f->which < 0 || (mode == FSOM_OPEN_EXISTING && !G.exists[f->which]) ? FSE_NOT_EXIST : FSE_OK;
    if (!f->err && mode == FSOM_OPEN_APPEND) { G.exists[f->which] = true; f->pos = strlen(buffer(f->which)); }
    return !f->err;
}
bool storage_file_close(File *f) { (void)f; return true; }
size_t storage_file_read(File *f, void *buf, size_t n) {
    const char *b = f->err ? "" : buffer(f->which); size_t len = strlen(b), k = f->pos < len ? len - f->pos : 0;
    if (k > n) k = n;
    memcpy(buf, b + f->pos, k); f->pos += k; return k;
}
size_t storage_file_write(File *f, const void *buf, size_t n) {
    char *b = buffer(f->which); size_t len = strlen(b), lines = 0;
    for (const char *p = b; (p = strchr(p, '\n')); p++) lines++;
    if (f->err || len + n >= FAV_CAP || lines >= FILES_FAV_MAX) { f->err = FSE_DENIED; return 0; }
    memcpy(b + len, buf, n); b[len + n] = 0; f->pos = len + n; G.dirty |= !f->which; return n;
}
uint64_t storage_file_tell(File *f) { return f->pos; }
bool storage_file_seek(File *f, uint32_t offset, bool from_start) { (void)from_start; f->pos = offset; return true; }
FS_Error storage_file_get_error(File *f) { return f->err; }
bool storage_common_exists(Storage *s, const char *path) { (void)s; int w = which(path); return w >= 0 && G.exists[w]; }
/* Only "not found" drops a favourite: a card that cannot be read (lent, missing) keeps them all. */
static bool on_card(const char *path) { /* a folder's line ends with "/" */
    ink_fs_stat_t st; char p[INK_FS_PATH_MAX + 1]; snprintf(p, sizeof p, "%s", path); size_t n = strlen(p);
    if (n > 1 && p[n - 1] == '/') p[n - 1] = 0;
    return ink_fs_stat(p, &st) != INK_FS_ERR_NOT_FOUND;
}
static bool listed(const char *lines, const char *path) {
    size_t n = strlen(path);
    for (const char *p = lines; (p = strstr(p, path)); p++) if ((p == lines || p[-1] == '\n') && p[n] == '\n') return true;
    return false;
}
/* During files_fl_rescan the card was read before the lock was taken: a line found gone then is dropped, a line added
 * since is kept. */
bool storage_file_exists(Storage *s, const char *path) {
    (void)s;
    return G.scan_task == osal_task_self() && G.scan_epoch == G.epoch ? !listed(G.gone, path) : on_card(path);
}
FS_Error storage_common_remove(Storage *s, const char *path) {
    (void)s; int w = which(path);
    if (w < 0) return G.removed ? FSE_OK : FSE_DENIED; /* the file service's receipt, see files_fl_deleted */
    buffer(w)[0] = 0; G.exists[w] = false; G.dirty |= !w; return FSE_OK;
}
FS_Error storage_common_rename(Storage *s, const char *from, const char *to) {
    (void)s;
    if (which(from) != 1 || which(to)) return FSE_DENIED;
    memcpy(G.prefs.fav, G.tmp, FAV_CAP); G.tmp[0] = 0; G.exists[0] = true; G.exists[1] = false; G.dirty = true; return FSE_OK;
}
FS_Error storage_common_stat(Storage *s, const char *path, FileInfo *info) { (void)s; (void)path; info->flags = G.dir; return FSE_OK; }
bool storage_simply_remove_recursive(Storage *s, const char *path) { return storage_common_remove(s, path) == FSE_OK; }

/* The menu: render_item_menu's text, kept by name; "---" and the favourites reorder ("Move", not wired) drop out. */
void canvas_draw_str(Canvas *canvas, int x, int y, const char *text) {
    static const char *const known[] = {"Run in app", "Pin", "Unpin", "Rename", "Delete", "Copy", "Move to", "Info", "Edit", "Read", "Pin folder", "Unpin folder"};
    (void)canvas; (void)x; (void)y;
    for (size_t i = 0; i < sizeof known / sizeof *known; i++) if (!strcmp(text, known[i]) && G.menu_n < FILES_MENU_MAX) G.menu[G.menu_n++] = known[i];
}
bool archive_is_item_in_array(ArchiveBrowserViewModel *m, uint32_t idx) { return (int32_t)idx >= m->array_offset && idx < m->array_offset + m->files->count; }
ArchiveFile_t *archive_get_current_file(ArchiveBrowserView *b) { (void)b; return G.has ? &G.file : NULL; }
ArchiveTabEnum archive_get_tab(ArchiveBrowserView *b) { (void)b; return G.model.tab_idx; }
bool archive_is_home(ArchiveBrowserView *b) { (void)b; return files_fl_home(); }
const char *archive_get_name(ArchiveBrowserView *b) { (void)b; return furi_string_get_cstr(G.file.path); }
void archive_show_file_menu(ArchiveBrowserView *b, bool show) { (void)b; files_fl_show_menu(show); }
void archive_file_array_rm_selected(ArchiveBrowserView *b) { (void)b; files_fl_removed(); }
void archive_file_array_swap(ArchiveBrowserView *b, int8_t dir) { (void)b; (void)dir; } /* favourites reorder: not wired */
void archive_file_array_load(ArchiveBrowserView *b, int8_t dir) { (void)b; files_fl_turn(dir); }
void archive_favorites_move_mode(ArchiveBrowserView *b, bool active) { (void)b; (void)active; }
void archive_update_focus(ArchiveBrowserView *b, const char *target) { (void)b; (void)target; } /* files_fl_leave restores it */
void archive_enter_dir(ArchiveBrowserView *b, FuriString *name) { (void)b; files_fl_enter(furi_string_get_cstr(name)); }
void archive_leave_dir(ArchiveBrowserView *b) { (void)b; files_fl_leave(); }
void archive_refresh_dir(ArchiveBrowserView *b) { (void)b; files_fl_reload(); }
bool archive_favorites_read(void *context) { (void)context; files_fl_reload(); return true; }
void archive_favorites_save(void *context) { (void)context; }
void scene_manager_next_scene(SceneManager *m, uint32_t scene) { (void)m; files_fl_scene((int)scene); }
void view_dispatcher_stop(ViewDispatcher *d) { (void)d; files_fl_stop(); }
void loader_start_with_gui_error(Loader *l, const char *name, const char *args) { (void)l; (void)args; files_fl_open(name); }
static void send(ArchiveBrowserEvent event, void *context) { archive_scene_browser_on_event(context, (SceneManagerEvent){SceneManagerEventTypeCustom, event}); }

int files_fl_init(void) {
    ArchiveFile_t_init(&G.file); G.app.fav_move_str = furi_string_alloc();
    G.app.browser = &G.view; G.view.callback = send; G.view.context = &G.app; G.model.files->items = &G.file; G.model.files->count = 1;
    return G.file.path && G.app.fav_move_str ? 0 : -1;
}
/* Folders are favourites only by an exact line, "path/" (an addition: the original cannot pin folders, and its prefix
 * match would make a folder look pinned for any pinned file inside it). */
static char *line_of(const char *path) {
    char want[INK_FS_PATH_MAX + 2]; size_t n = (size_t)snprintf(want, sizeof want, "%s/", path);
    for (char *p = G.prefs.fav; path[0] && (p = strstr(p, want)); p++) if ((p == G.prefs.fav || p[-1] == '\n') && p[n] == '\n') return p;
    return NULL;
}
bool files_fl_is_fav(const char *path, bool dir) {
    if (!dir) return archive_is_favorite("%s", path);
    files_prefs_begin(); bool on = line_of(path); files_prefs_end(false); return on;
}
void files_fl_pin_folder(const char *path) {
    files_prefs_begin(); char *at = line_of(path);
    if (at) memmove(at, at + strlen(path) + 2, strlen(at + strlen(path) + 2) + 1);
    files_prefs_end(at != NULL);
    char line[INK_FS_PATH_MAX + 2]; snprintf(line, sizeof line, "%s/", path);
    if (!at) archive_add_to_favorites(line);
}
void files_fl_select(const char *path, bool dir, bool favorites) {
    furi_string_set_str(G.file.path, path); G.dir = dir; G.has = path[0];
    archive_set_file_type(&G.file, path, dir, false); G.file.fav = G.has && files_fl_is_fav(path, dir);
    G.model.tab_idx = favorites ? ArchiveTabFavorites : ArchiveTabBrowser;
}
int files_fl_menu(const char *out[FILES_MENU_MAX]) {
    G.menu_n = 0; ink_flipper_menu(&G.model);
    memcpy(out, G.menu, sizeof G.menu); return G.menu_n;
}
bool files_fl_event(int event) { return archive_scene_browser_on_event(&G.app, (SceneManagerEvent){SceneManagerEventTypeCustom, (uint32_t)event}); }
int files_fl_type(const char *path, bool dir) {
    ArchiveFile_t f = {.path = furi_string_alloc_set_str(path)};
    if (f.path) archive_set_file_type(&f, path, dir, false);
    furi_string_free(f.path); return f.path ? (int)f.type : FILES_TYPE_UNKNOWN;
}
/* Flipper's rename scene: a pinned file's favourite follows it (folders are left to the rescan, as there). */
void files_fl_renamed(const char *from, const char *to) { if (archive_is_favorite("%s", from)) archive_favorites_rename(from, to); }
/* After file.delete's receipt (the card and the confirmation are asynchronous here): remove, unpin, take off the list. */
void files_fl_deleted(const char *path, bool removed) { G.removed = removed; files_fl_select(path, G.dir, false); archive_delete_file(&G.view, "%s", path); }
/* The card is read with the lock free, so page creation (which reads the sort) never waits on it; Flipper's rescan
 * then runs as it is, under the lock, in memory, and writes only when it dropped a line. */
bool files_fl_rescan(void) {
    files_prefs_begin(); memcpy(G.lines, G.prefs.fav, FAV_CAP); G.scan_epoch = G.epoch; files_prefs_end(false);
    G.gone[0] = 0;
    for (char *line = G.lines, *end; (end = strchr(line, '\n')); line = end + 1) {
        *end = 0;
        if (line[0] && !on_card(line) && strlen(G.gone) + strlen(line) + 2 < sizeof G.gone) { strcat(G.gone, line); strcat(G.gone, "\n"); }
    }
    G.scan_task = osal_task_self(); bool r = ink_flipper_rescan(); G.scan_task = NULL;
    return r;
}
