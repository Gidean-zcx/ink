#define INK_MOGO_PORT_IMPL
#include <runtime.h>
#include <book_index_cache.h>
#include <book_progress.h>
#include <epdiy.h>
#include <stdarg.h>
#include <settings.h>
#include <nvs.h>
#include "ink/sys/time.h"
#include <page.h>
#ifndef ESP_PLATFORM
#include <vtk.h>
#endif
/* An open book: a member file, or a bundled asset ("asset:sample/welcome.txt") read into PSRAM, read-only. */
typedef struct { ink_fs_file_t *file; bool error; uint8_t *mem; size_t len, pos; } ink_mogo_stream_t;
/* Host work (formatting, sorting) from a simulated task goes through the host; on the device it is a call. */
static void ink_mogo_host_call(void (*fn)(void *), void *arg) {
#ifdef ESP_PLATFORM
    fn(arg);
#else
    vtk_host_call(fn, arg);
#endif
}
/* The reader's font, in one place (auto-provision-1 puts the family's copy there). */
static const char INK_MOGO_FONT_OWN[] = "/me/fonts/InkSC.ttf", INK_MOGO_FONT_FAMILY[] = "/family/fonts/InkSC.ttf";
static const char *ink_mogo_font = INK_MOGO_FONT_FAMILY;
static ink_member_identity_t ink_mogo_member;
static ink_fs_file_t *ink_mogo_fds[4];
static int ink_mogo_w, ink_mogo_h, ink_mogo_px;
const uint8_t ink_mogo_builtin[32] asm("_binary_builtin_ttf_start") = {0};
asm(".globl _binary_builtin_ttf_end\n.set _binary_builtin_ttf_end, _binary_builtin_ttf_start + 32");
void *ink_mogo_malloc(size_t n) { return ink_app_alloc(n, true); }
void *ink_mogo_calloc(size_t n, size_t s) {
    if (s && n > SIZE_MAX / s) return NULL;
    void *p = ink_mogo_malloc(n * s);
    if (p) memset(p, 0, n * s);
    return p;
}
size_t ink_mogo_free(void) { size_t p; ink_app_mem_free(NULL, NULL, &p); return p; }
void ink_mogo_owner(const ink_member_identity_t *o) { ink_mogo_member = *o; }
const ink_member_identity_t *ink_mogo_current_owner(void) { return &ink_mogo_member; }
void ink_mogo_dimensions(int w, int h, int px) { ink_mogo_w = w; ink_mogo_h = h; ink_mogo_px = px; ink_mogo_canvas(w,h); }
ink_mogo_settings_t ink_mogo_settings;
int ink_mogo_scaled(int px) { return (px * ink_mogo_px + 50) / 100; }
int app_settings_system_font_size(void) { return ink_mogo_px; }
int app_settings_book_px(void) { return ink_mogo_scaled(ink_mogo_settings.px); }
void ink_mogo_settings_load(void) {
    ink_mogo_settings = (ink_mogo_settings_t){.px=48,.line=150,.paragraph=50,.margin={36,36,0,0},.indent=2};
    nvs_handle_t h;
    if (nvs_open("reading",NVS_READONLY,&h)) return;
    ink_mogo_settings_t p;
    size_t n = sizeof p;
    if (!nvs_get_blob(h,"layout",&p,&n) && n == sizeof p && p.px >= 18 && p.px <= 90 &&
        p.line >= 110 && p.line <= 150 && p.paragraph <= 100 && p.indent <= 3 &&
        p.margin[0] <= 96 && p.margin[1] <= 96 && p.margin[2] <= 96 && p.margin[3] <= 96 &&
        p.header <= 1 && p.footer <= 1 && p.weight >= -32 && p.weight <= 32) ink_mogo_settings = p;
    nvs_close(h);
}
bool ink_mogo_settings_save(void) {
    nvs_handle_t h;
    if (nvs_open("reading",NVS_READWRITE,&h)) return false;
    int err = nvs_set_blob(h,"layout",&ink_mogo_settings,sizeof ink_mogo_settings);
    if (!err) err = nvs_commit(h);
    nvs_close(h);
    return !err;
}
const char *app_settings_fonts_dir(void) { return "/me/fonts"; }
/* A member's own copy wins; otherwise the family's one copy (on the card: fonts/InkSC.ttf). */
int ink_mogo_font_check(void) {
    ink_fs_file_t *f = NULL;
    if (!ink_graft_file_open(&ink_mogo_member, INK_MOGO_FONT_OWN, INK_FS_READ, &f)) ink_mogo_font = INK_MOGO_FONT_OWN;
    else if (!ink_graft_file_open(&ink_mogo_member, INK_MOGO_FONT_FAMILY, INK_FS_READ, &f)) ink_mogo_font = INK_MOGO_FONT_FAMILY;
    else return INK_MOGO_R_FILES; /* no card, lent, missing: the card files' notice says which */
    ink_fs_close(f);
    return INK_MOGO_R_NONE;
}
/* The member's own copy would not load: the family's copy, if it is there (that one the card files look after). */
bool ink_mogo_font_family(void) {
    ink_fs_file_t *f = NULL;
    if (ink_mogo_font != INK_MOGO_FONT_OWN || ink_graft_file_open(&ink_mogo_member, INK_MOGO_FONT_FAMILY, INK_FS_READ, &f)) return false;
    ink_fs_close(f); ink_mogo_font = INK_MOGO_FONT_FAMILY;
    return true;
}
const char *app_settings_font_path(void) { return ink_mogo_font; }
static ink_mogo_stream_t *ink_mogo_asset(const char *path) {
    int64_t size = ink_app_asset_read(path + 6, NULL, 0);
    ink_mogo_stream_t *stream = size > 0 ? ink_mogo_calloc(1, sizeof *stream) : NULL;
    if (stream && (!(stream->mem = ink_mogo_malloc((size_t)size)) || ink_app_asset_read(path + 6, stream->mem, (size_t)size) != size)) {
        ink_app_free(stream->mem); ink_app_free(stream); return NULL;
    }
    if (stream) stream->len = (size_t)size;
    return stream;
}
FILE *ink_mogo_fopen(const char *path, const char *mode) {
    ink_fs_file_t *f = NULL;
    if (!strcmp(mode, "rb") && !strncmp(path, "asset:", 6)) return (FILE *)ink_mogo_asset(path);
    if (strcmp(mode, "rb") || ink_graft_file_open(&ink_mogo_member, path, INK_FS_READ, &f)) return NULL;
    ink_mogo_stream_t *stream = ink_mogo_malloc(sizeof *stream);
    if (!stream) { ink_fs_close(f); return NULL; }
    *stream = (ink_mogo_stream_t){.file = f};
    return (FILE *)stream;
}
size_t ink_mogo_fread(void *p, size_t s, size_t n, FILE *f) {
    if (!s || n > SIZE_MAX / s) return 0;
    ink_mogo_stream_t *stream = (ink_mogo_stream_t *)f;
    if (stream->mem) {
        size_t got = s * n < stream->len - stream->pos ? s * n : stream->len - stream->pos;
        memcpy(p, stream->mem + stream->pos, got); stream->pos += got; return got / s;
    }
    int64_t got = ink_fs_read(stream->file, p, s * n);
    if (got < 0) stream->error = true;
    return got > 0 ? (size_t)got / s : 0;
}
size_t ink_mogo_fwrite(const void *p, size_t s, size_t n, FILE *f) { (void)p; (void)s; (void)n; (void)f; return 0; }
/* The lock styles' "wb" files (apps/mogo_lock/port): the same stream, so fread/fseek/fclose above take them too. */
FILE *ink_mogo_stream_wrap(ink_fs_file_t *f) {
    ink_mogo_stream_t *stream = ink_mogo_malloc(sizeof *stream);
    if (stream) *stream = (ink_mogo_stream_t){.file = f};
    return (FILE *)stream;
}
size_t ink_mogo_stream_write(const void *p, size_t s, size_t n, FILE *f) {
    ink_mogo_stream_t *stream = (ink_mogo_stream_t *)f;
    if (!s || n > SIZE_MAX / s || stream->mem) return 0;
    int64_t put = ink_fs_write(stream->file, p, s * n);
    if (put < 0) { stream->error = true; return 0; }
    return (size_t)put / s;
}
int ink_mogo_fclose(FILE *f) { ink_mogo_stream_t *s = (ink_mogo_stream_t *)f; int r = s->mem ? 0 : ink_fs_close(s->file); ink_app_free(s->mem); ink_app_free(s); return r; }
int ink_mogo_ferror(FILE *f) { return ((ink_mogo_stream_t *)f)->error; }
void ink_mogo_clearerr(FILE *f) { ((ink_mogo_stream_t *)f)->error = false; }
int ink_mogo_fgetc(FILE *f) { unsigned char c; return ink_mogo_fread(&c,1,1,f) == 1 ? c : EOF; }
int ink_mogo_fseek(FILE *f, long off, int whence) {
    ink_mogo_stream_t *s = (ink_mogo_stream_t *)f;
    if (s->mem) {
        long at = whence == SEEK_SET ? off : whence == SEEK_CUR ? (long)s->pos + off : (long)s->len + off;
        if (at < 0 || (size_t)at > s->len) return -1;
        s->pos = (size_t)at; return 0;
    }
    int r = ink_fs_seek(s->file, off, whence);
    if (r) s->error = true;
    return r;
}
long ink_mogo_ftell(FILE *f) { ink_mogo_stream_t *s = (ink_mogo_stream_t *)f; return s->mem ? (long)s->pos : (long)ink_fs_tell(s->file); }
int ink_mogo_stat(const char *path, struct stat *st) {
    ink_fs_file_t *f;
    if (!strncmp(path, "asset:", 6)) {
        int64_t size = ink_app_asset_read(path + 6, NULL, 0);
        memset(st, 0, sizeof *st); st->st_mode = S_IFREG; st->st_size = size;
        return size > 0 ? 0 : -1;
    }
    if (ink_graft_file_open(&ink_mogo_member, path, INK_FS_READ, &f)) return -1;
    int err = ink_fs_seek(f, 0, SEEK_END);
    int64_t size = ink_fs_tell(f);
    int closed = ink_fs_close(f);
    memset(st, 0, sizeof *st);
    st->st_mode = S_IFREG; st->st_size = size;
    return err || closed || size < 0 ? -1 : 0;
}
int ink_mogo_open(const char *path, int flags, ...) {
    if (flags != O_RDONLY) return -1;
    for (size_t i = 0; i < sizeof ink_mogo_fds / sizeof *ink_mogo_fds; ++i) {
        if (!ink_mogo_fds[i] && !ink_graft_file_open(&ink_mogo_member, path, INK_FS_READ, &ink_mogo_fds[i])) return (int)i;
    }
    return -1;
}
int ink_mogo_close(int fd) { int r = ink_fs_close(ink_mogo_fds[fd]); ink_mogo_fds[fd] = NULL; return r; }
ssize_t ink_mogo_read(int fd, void *p, size_t n) { return (ssize_t)ink_fs_read(ink_mogo_fds[fd], p, n); }
off_t ink_mogo_lseek(int fd, off_t off, int whence) {
    return ink_fs_seek(ink_mogo_fds[fd], off, whence) ? -1 : (off_t)ink_fs_tell(ink_mogo_fds[fd]);
}
void epd_draw_pixel(int x, int y, uint8_t gray, uint8_t *fb) {
    if (x >= 0 && x < ink_mogo_w && y >= 0 && y < ink_mogo_h) fb[(size_t)y * ink_mogo_w + x] = gray | (gray >> 4);
}
void epd_fill_rect(EpdRect r, uint8_t gray, uint8_t *fb) {
    for (int y = r.y; y < r.y + r.height; ++y)
        for (int x = r.x; x < r.x + r.width; ++x) epd_draw_pixel(x, y, gray, fb);
}
typedef struct { char *out; size_t cap; const char *fmt; va_list args; int result; } ink_mogo_format_t;
static void ink_mogo_format(void *arg) { ink_mogo_format_t *f = arg; f->result = vsnprintf(f->out, f->cap, f->fmt, f->args); }
int ink_mogo_snprintf(char *out, size_t cap, const char *fmt, ...) {
    ink_mogo_format_t f = {.out = out, .cap = cap, .fmt = fmt};
    va_start(f.args, fmt); ink_mogo_host_call(ink_mogo_format, &f); va_end(f.args); return f.result;
}
bool book_index_cache_prepare(const char *p, const char *tag, uint32_t v, char *o, size_t n, book_index_cache_header_t *k) {
    (void)p; (void)tag; (void)v; (void)k; (void)o; (void)n; return false;
}
FILE *book_index_cache_open_read(const char *p, const book_index_cache_header_t *h) { (void)p; (void)h; return NULL; }
FILE *book_index_cache_open_write(const char *p, const book_index_cache_header_t *h, char *t, size_t n) { (void)p; (void)h; (void)t; (void)n; return NULL; }
bool book_index_cache_finish_write(FILE *f, const char *t, const char *p, bool ok) { (void)f; (void)t; (void)p; (void)ok; return false; }

time_t ink_mogo_time(time_t *out) { time_t now=(time_t)ink_time_now_s(); if (out) *out=now; return now; }
typedef struct { void *base; size_t count,size; int (*compare)(const void *,const void *); } ink_mogo_sort_t;
static void ink_mogo_host_sort(void *arg) { ink_mogo_sort_t *s=arg; qsort(s->base,s->count,s->size,s->compare); }
void ink_mogo_qsort(void *base,size_t count,size_t size,int (*compare)(const void *,const void *)) {
    ink_mogo_sort_t sort={base,count,size,compare}; ink_mogo_host_call(ink_mogo_host_sort,&sort);
}
void ink_mogo_dimensions_get(int *w, int *h, int *px) { *w = ink_mogo_w; *h = ink_mogo_h; *px = ink_mogo_px; } /* lock-styles-1: the lock styles put the page's size back */
