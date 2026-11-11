#ifndef INK_MUSIC_IO_H
#define INK_MUSIC_IO_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ink/osal.h"
#include "ink/sys/file.h"
static inline void *ink_music_malloc(size_t n) { return osal_malloc(n, OSAL_MEM_EXTERNAL); }
static inline void *ink_music_realloc(void *p, size_t n) { return osal_realloc(p, n, OSAL_MEM_EXTERNAL); }
enum { INK_MUSIC_INPUT_BYTES = 4096 };
typedef struct { ink_fs_file_t *file; size_t at, end; uint8_t data[INK_MUSIC_INPUT_BYTES]; } ink_music_file_t;
static inline ink_music_file_t *ink_music_open_for(const char *member, const char *path, int *err) {
    ink_music_file_t *f = osal_calloc(1, sizeof *f, OSAL_MEM_EXTERNAL);
    *err = f ? (member ? ink_fs_open_for(member, path, INK_FS_READ, &f->file) : ink_fs_open(path, INK_FS_READ, &f->file)) : INK_FS_ERR_BUSY;
    if (*err) { osal_free(f); return NULL; }
    return f;
}
static inline ink_music_file_t *ink_music_fopen(const char *p, const char *mode) {
    (void)mode; int e; return ink_music_open_for(NULL, p, &e);
}
static inline int ink_music_fclose(ink_music_file_t *f) { int e = ink_fs_close(f->file); osal_free(f); return e; }
static inline int ink_music_fseek(ink_music_file_t *f, int64_t off, int origin) {
    if (origin == SEEK_CUR) off -= (int64_t)(f->end - f->at);
    int e = ink_fs_seek(f->file, off, origin);
    if (!e) f->at = f->end = 0;
    return e;
}
static inline int64_t ink_music_ftell(ink_music_file_t *f) { return ink_fs_tell(f->file) - (int64_t)(f->end - f->at); }
static inline size_t ink_music_fread(void *p, size_t size, size_t n, ink_music_file_t *f) {
    size_t want = size * n, done = 0;
    while (done < want) {
        if (f->at == f->end) {
            int64_t got = ink_fs_read(f->file, f->data, sizeof f->data);
            if (got <= 0) break;
            f->at = 0; f->end = (size_t)got;
        }
        size_t copy = f->end - f->at;
        if (copy > want - done) copy = want - done;
        memcpy((uint8_t *)p + done, f->data + f->at, copy); f->at += copy; done += copy;
    }
    return size ? done / size : 0;
}
static inline int ink_music_fgetc(ink_music_file_t *f) { unsigned char c; return ink_music_fread(&c, 1, 1, f) == 1 ? c : EOF; }
static inline size_t ink_music_strlcpy(char *out, const char *in, size_t n) {
    size_t len = strlen(in), copy = n && len >= n ? n - 1 : len;
    if (n) { memcpy(out, in, copy); out[copy] = 0; }
    return len;
}
#define FILE ink_music_file_t
#define fopen ink_music_fopen
#define fclose ink_music_fclose
#define fread ink_music_fread
#define fseek ink_music_fseek
#define ftell ink_music_ftell
#define fgetc ink_music_fgetc
#define malloc ink_music_malloc
#define realloc ink_music_realloc
#define free osal_free
#undef strlcpy
#define strlcpy ink_music_strlcpy
#endif
