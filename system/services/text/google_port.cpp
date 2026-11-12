#define INK_GP_NO_REDIRECT
#include "google_port.h"
#include "ink/hal_storage.h"
#include "ink/sys/file.h"
#include "ink/probe.h"
static constexpr size_t USER_BYTES = 65536;
struct ink_gp_file { uint8_t *data; size_t size, pos, cap; bool writing, failed; };
static ink_gp_file *writer;
void *ink_gp_alloc(size_t n) { const char *tag = osal_mem_tag("google-ime"); void *p = osal_malloc(n, OSAL_MEM_EXTERNAL); osal_mem_tag(tag); return p; }
char *ink_gp_strdup(const char *s) { char *p = (char *)ink_gp_alloc(strlen(s) + 1); if (p) strcpy(p, s); return p; }
ink_gp_file *ink_gp_fopen(const char *path, const char *mode) {
    bool system = !strcmp(path, "/content/ime/dict_pinyin.dat"), writing = strchr(mode, '+') || strchr(mode, 'w');
    if (!system && strcmp(path, "/me/ime/google.user")) return nullptr;
    hal_file_t *sys = nullptr; ink_fs_file_t *user = nullptr;
    int64_t size = 0;
    bool held = system && ink_fs_card_hold() == 0;
    if (system) { size = held ? hal_fs_size(path) : -1; if (size > 0 && size <= 1068442) sys = hal_fs_open(path, HAL_O_READ); }
    else if (!strchr(mode, 'w') && ink_fs_open(path, INK_FS_READ, &user) == 0) {
        if (ink_fs_seek(user, 0, SEEK_END) == 0) size = ink_fs_tell(user);
        if (ink_fs_seek(user, 0, SEEK_SET) != 0) size = -1;
    } else if (!strchr(mode, 'w')) size = -1;
    ink_gp_file *f = nullptr;
    if ((system ? sys != nullptr : size >= 0 && size <= (int64_t)USER_BYTES)) {
        f = (ink_gp_file *)osal_calloc(1, sizeof *f, OSAL_MEM_EXTERNAL);
        if (f) {
            f->cap = system ? (size_t)size : USER_BYTES; f->size = (size_t)size; f->writing = writing;
            f->data = (uint8_t *)ink_gp_alloc(f->cap);
            size_t got = 0;
            while (f->data && got < f->size) {
                int64_t n = system ? hal_fs_read(sys, f->data + got, f->size - got) : ink_fs_read(user, f->data + got, f->size - got);
                if (n <= 0) break;
                got += (size_t)n;
            }
            if (!f->data || got != f->size) { osal_free(f->data); osal_free(f); f = nullptr; }
        }
    }
    if (sys) hal_fs_close(sys);
    if (held) ink_fs_card_release();
    if (user) ink_fs_close(user);
    return f;
}
size_t ink_gp_fread(void *p, size_t size, size_t n, ink_gp_file *f) {
    if (!size) return 0;
    if (n > (f->size - f->pos) / size) n = (f->size - f->pos) / size;
    memcpy(p, f->data + f->pos, size * n); f->pos += size * n; return n;
}
size_t ink_gp_fwrite(const void *p, size_t size, size_t n, ink_gp_file *f) {
    if (!f->writing || (size && n > (f->cap - f->pos) / size)) { f->failed = true; return 0; }
    memcpy(f->data + f->pos, p, size * n); f->pos += size * n;
    if (f->pos > f->size) f->size = f->pos;
    return n;
}
int ink_gp_fseek(ink_gp_file *f, long off, int whence) {
    int64_t pos = off + (whence == SEEK_SET ? 0 : whence == SEEK_END ? (int64_t)f->size : (int64_t)f->pos);
    if (pos < 0 || (uint64_t)pos > f->size) return -1;
    f->pos = (size_t)pos; return 0;
}
int ink_gp_feof(ink_gp_file *f) { return f->pos == f->size; }
int ink_gp_ferror(ink_gp_file *f) { return f->failed; }
long ink_gp_ftell(ink_gp_file *f) { return (long)f->pos; }
int ink_gp_fclose(ink_gp_file *f) {
    int e = f->failed ? -1 : f->writing ? ink_fs_write_atomic("/me/ime/google.user", f->data, f->size) : 0;
    if (e) ink_probe_count("text.google_write_failed");
    osal_free(f->data); osal_free(f); return e;
}
int ink_gp_open(const char *path, int flags) {
    (void)flags;
    if (writer) return -1;
    writer = ink_gp_fopen(path, "r+"); return writer ? 1 : -1;
}
ssize_t ink_gp_write(int fd, const void *p, size_t n) { return fd == 1 && writer ? (ssize_t)ink_gp_fwrite(p, 1, n, writer) : -1; }
off_t ink_gp_lseek(int fd, off_t off, int whence) { return fd == 1 && writer && !ink_gp_fseek(writer, off, whence) ? (off_t)writer->pos : -1; }
int ink_gp_truncate(int fd, off_t n) { if (fd != 1 || !writer || n < 0 || (size_t)n > writer->cap) return -1; writer->size = (size_t)n; return 0; }
int ink_gp_close(int fd) { if (fd != 1 || !writer) return -1; ink_gp_file *f = writer; writer = nullptr; return ink_gp_fclose(f); }
int ink_gp_unlink(const char *path) { return ink_fs_remove(path); }
int ink_gp_mutex_lock(osal_mutex_t **m, bool wait) {
    if (!*m) *m = osal_mutex_create();
    return *m && osal_mutex_lock(*m, wait ? OSAL_WAIT_FOREVER : OSAL_NO_WAIT) == OSAL_OK ? 0 : -1;
}
int ink_gp_mutex_unlock(osal_mutex_t **m) { osal_mutex_unlock(*m); return 0; }
int ink_gp_gettimeofday(timeval *v, void *unused) { (void)unused; int64_t t = osal_wall_us(); if (!t) t = 1262304000000000LL + (int64_t)osal_now_us(); v->tv_sec = t / 1000000; v->tv_usec = t % 1000000; return 0; }
time_t ink_gp_time(time_t *out) { timeval v; ink_gp_gettimeofday(&v, nullptr); if (out) *out = v.tv_sec; return v.tv_sec; }
