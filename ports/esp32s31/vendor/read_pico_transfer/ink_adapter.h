/* File-service boundary for kiikoread@746ce31; no libc storage access. */
#pragma once
#define INK_TRANSFER 1
#include "ink/hal_transfer.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static const hal_transfer_policy_t *ink_policy;
static int64_t ink_io(hal_transfer_io_t *io) {
    int64_t r = ink_policy->file(io);
    errno = r >= 0 ? 0 : r == -3 ? ENOENT : r == -4 ? EEXIST : r == -7 ? ENOSPC : r == -2 ? EACCES : EIO;
    return r;
}
static const char *ink_path(const char *p) {
    return !strncmp(p, "/sdcard", 7) && (!p[7] || p[7] == '/') ? p + 7 : "/../denied";
}
typedef struct { void *handle; bool error, eof, writing; } ink_file_t;
static ink_file_t *ink_fopen(const char *path, const char *mode) {
    hal_transfer_io_t io = {.op = HAL_TRANSFER_OPEN, .path = ink_path(path), .value = *mode == 'w'};
    ink_file_t *f = heap_caps_calloc(1, sizeof *f, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!f) return NULL;
    if (ink_io(&io)) { free(f); return NULL; }
    f->handle = io.handle; f->writing = io.value; return f;
}
static size_t ink_rw(void *data, size_t unit, size_t n, ink_file_t *f, int op) {
    hal_transfer_io_t io = {.op = op, .handle = f->handle, .data = data, .len = unit * n};
    int64_t r = ink_io(&io);
    if (r < 0) { f->error = true; return 0; }
    if (!r) f->eof = true;
    return unit ? (size_t)r / unit : 0;
}
static int ink_fclose(ink_file_t *f) {
    hal_transfer_io_t io = {.op = HAL_TRANSFER_SYNC, .handle = f->handle};
    int r = f->error || (f->writing && ink_io(&io));
    io.op = HAL_TRANSFER_CLOSE;
    if (ink_io(&io)) r = -1;
    free(f); return r ? -1 : 0;
}
static int ink_stat(const char *p, struct stat *st) {
    hal_transfer_io_t io = {.op = HAL_TRANSFER_STAT, .path = ink_path(p)};
    if (ink_io(&io)) return -1;
    memset(st, 0, sizeof *st); st->st_size = io.value; st->st_mode = io.directory ? S_IFDIR : S_IFREG; return 0;
}
static int ink_mutate(const char *p, const char *to, int op) {
    hal_transfer_io_t io = {.op = op, .path = ink_path(p), .to = to ? ink_path(to) : NULL};
    if (ink_io(&io)) return -1;
    size_t n = strlen(p);
    if (to && n > 5 && !strcmp(p + n - 5, ".part")) {
        io.op = HAL_TRANSFER_RECEIVED; io.path = ink_path(to); io.to = NULL; ink_io(&io);
    }
    return 0;
}
typedef struct { void *handle; char path[256]; struct dirent entry; } ink_dir_t;
static ink_dir_t *ink_opendir(const char *p) {
    hal_transfer_io_t io = {.op = HAL_TRANSFER_DIR_OPEN, .path = ink_path(p)};
    ink_dir_t *d = heap_caps_calloc(1, sizeof *d, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!d) return NULL;
    if (strlen(p) >= sizeof d->path || ink_io(&io)) { free(d); return NULL; }
    d->handle = io.handle; strcpy(d->path, p); return d;
}
static struct dirent *ink_readdir(ink_dir_t *d) {
    hal_transfer_io_t io = {.op = HAL_TRANSFER_DIR_NEXT, .handle = d->handle};
    if (ink_io(&io) != 1) return NULL;
    snprintf(d->entry.d_name, sizeof d->entry.d_name, "%s", io.name); return &d->entry;
}
static int ink_closedir(ink_dir_t *d) {
    hal_transfer_io_t io = {.op = HAL_TRANSFER_DIR_CLOSE, .handle = d->handle};
    int r = (int)ink_io(&io); free(d); return r;
}
static void ink_rewinddir(ink_dir_t *d) {
    hal_transfer_io_t io = {.op = HAL_TRANSFER_DIR_CLOSE, .handle = d->handle}; ink_io(&io);
    io.op = HAL_TRANSFER_DIR_OPEN; io.path = ink_path(d->path); io.handle = NULL; ink_io(&io); d->handle = io.handle;
}
static bool read_pico_search_match(const char *name, const char *query) { return !*query || strstr(name, query); }
#define FILE ink_file_t
#define DIR ink_dir_t
#define fopen ink_fopen
#define fclose ink_fclose
#define fread(p,u,n,f) ink_rw(p,u,n,f,HAL_TRANSFER_READ)
#define fwrite(p,u,n,f) ink_rw((void *)(p),u,n,f,HAL_TRANSFER_WRITE)
#define feof(f) ((f)->eof)
#define ferror(f) ((f)->error)
#define setvbuf(f,p,m,n) (0)
#define stat(p,s) ink_stat(p,s)
#define opendir ink_opendir
#define readdir ink_readdir
#define closedir ink_closedir
#define rewinddir ink_rewinddir
#define remove(p) ink_mutate(p,NULL,HAL_TRANSFER_REMOVE)
#define unlink(p) ink_mutate(p,NULL,HAL_TRANSFER_REMOVE)
#define rmdir(p) ink_mutate(p,NULL,HAL_TRANSFER_REMOVE)
#define mkdir(p,m) ink_mutate(p,NULL,HAL_TRANSFER_MKDIR)
#define rename(p,t) ink_mutate(p,t,HAL_TRANSFER_MOVE)
