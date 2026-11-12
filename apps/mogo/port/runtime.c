#define INK_MOGO_PORT_IMPL
#include <runtime.h>
#include <book_index_cache.h>
#include <book_progress.h>
#include <epdiy.h>
#include <stdarg.h>
typedef struct { ink_fs_file_t *file; bool error; } ink_mogo_stream_t;
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
void ink_mogo_dimensions(int w, int h, int px) { ink_mogo_w = w; ink_mogo_h = h; ink_mogo_px = px; }
int app_settings_book_px(void) { return ink_mogo_px; }
const char *app_settings_fonts_dir(void) { return "/me/fonts"; }
const char *app_settings_font_path(void) { return "/me/fonts/InkSC.ttf"; }
FILE *ink_mogo_fopen(const char *path, const char *mode) {
    ink_fs_file_t *f = NULL;
    if (strcmp(mode, "rb") || ink_graft_file_open(&ink_mogo_member, path, INK_FS_READ, &f)) return NULL;
    ink_mogo_stream_t *stream = ink_mogo_malloc(sizeof *stream);
    if (!stream) { ink_fs_close(f); return NULL; }
    *stream = (ink_mogo_stream_t){.file = f};
    return (FILE *)stream;
}
size_t ink_mogo_fread(void *p, size_t s, size_t n, FILE *f) {
    if (!s || n > SIZE_MAX / s) return 0;
    ink_mogo_stream_t *stream = (ink_mogo_stream_t *)f;
    int64_t got = ink_fs_read(stream->file, p, s * n);
    if (got < 0) stream->error = true;
    return got > 0 ? (size_t)got / s : 0;
}
size_t ink_mogo_fwrite(const void *p, size_t s, size_t n, FILE *f) { (void)p; (void)s; (void)n; (void)f; return 0; }
int ink_mogo_fclose(FILE *f) { ink_mogo_stream_t *s = (ink_mogo_stream_t *)f; int r = ink_fs_close(s->file); ink_app_free(s); return r; }
int ink_mogo_ferror(FILE *f) { return ((ink_mogo_stream_t *)f)->error; }
void ink_mogo_clearerr(FILE *f) { ((ink_mogo_stream_t *)f)->error = false; }
int ink_mogo_fgetc(FILE *f) { unsigned char c; return ink_mogo_fread(&c,1,1,f) == 1 ? c : EOF; }
int ink_mogo_fseek(FILE *f, long off, int whence) { ink_mogo_stream_t *s = (ink_mogo_stream_t *)f; int r = ink_fs_seek(s->file, off, whence); if (r) s->error = true; return r; }
long ink_mogo_ftell(FILE *f) { return (long)ink_fs_tell(((ink_mogo_stream_t *)f)->file); }
int ink_mogo_stat(const char *path, struct stat *st) {
    ink_fs_file_t *f;
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
extern void ink_reader_host_call(void (*fn)(void *), void *arg);
static void ink_mogo_format(void *arg) { ink_mogo_format_t *f = arg; f->result = vsnprintf(f->out, f->cap, f->fmt, f->args); }
int ink_mogo_snprintf(char *out, size_t cap, const char *fmt, ...) {
    ink_mogo_format_t f = {.out = out, .cap = cap, .fmt = fmt};
    va_start(f.args, fmt); ink_reader_host_call(ink_mogo_format, &f); va_end(f.args); return f.result;
}
bool book_index_cache_prepare(const char *p, const char *tag, uint32_t v, char *o, size_t n, book_index_cache_header_t *k) {
    (void)p; (void)tag; (void)v; (void)k; (void)o; (void)n; return false;
}
FILE *book_index_cache_open_read(const char *p, const book_index_cache_header_t *h) { (void)p; (void)h; return NULL; }
FILE *book_index_cache_open_write(const char *p, const book_index_cache_header_t *h, char *t, size_t n) { (void)p; (void)h; (void)t; (void)n; return NULL; }
bool book_index_cache_finish_write(FILE *f, const char *t, const char *p, bool ok) { (void)f; (void)t; (void)p; (void)ok; return false; }
bool book_progress_load(const char *p, uint32_t s, book_progress_t *v) { (void)p; (void)s; (void)v; return false; }
