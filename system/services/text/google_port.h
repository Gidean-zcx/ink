/* The vendor's libc surface, backed by OSAL and the member file service. */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <new>
#include "ink/osal.h"
struct ink_gp_file;
void *ink_gp_alloc(size_t n);
char *ink_gp_strdup(const char *s);
ink_gp_file *ink_gp_fopen(const char *path, const char *mode);
size_t ink_gp_fread(void *p, size_t size, size_t n, ink_gp_file *f);
size_t ink_gp_fwrite(const void *p, size_t size, size_t n, ink_gp_file *f);
int ink_gp_fseek(ink_gp_file *f, long offset, int whence);
long ink_gp_ftell(ink_gp_file *f);
int ink_gp_fclose(ink_gp_file *f);
int ink_gp_feof(ink_gp_file *f);
int ink_gp_ferror(ink_gp_file *f);
int ink_gp_open(const char *path, int flags);
ssize_t ink_gp_write(int fd, const void *p, size_t n);
off_t ink_gp_lseek(int fd, off_t offset, int whence);
int ink_gp_truncate(int fd, off_t n);
int ink_gp_close(int fd);
int ink_gp_unlink(const char *path);
int ink_gp_mutex_lock(osal_mutex_t **m, bool wait = true);
int ink_gp_mutex_unlock(osal_mutex_t **m);
int ink_gp_gettimeofday(timeval *v, void *unused);
time_t ink_gp_time(time_t *out);
template<class T> T *ink_gp_new(size_t n = 1) {
    T *p = static_cast<T *>(ink_gp_alloc(sizeof(T) * n));
    if (p) for (size_t i = 0; i < n; i++) ::new (p + i) T;
    return p;
}
template<class T> void ink_gp_delete(T *p) { if (p) { p->~T(); osal_free(p); } }
template<class T> size_t ink_gp_read_32(T *p, size_t n, ink_gp_file *f) {
    size_t i = 0;
    for (; i < n; i++) { uint32_t v; if (ink_gp_fread(&v, 4, 1, f) != 1) break; p[i] = v; }
    return i;
}
template<class T> size_t ink_gp_write_32(T *p, size_t n, ink_gp_file *f) {
    size_t i = 0;
    for (; i < n; i++) { uint32_t v = (uint32_t)p[i]; if (ink_gp_fwrite(&v, 4, 1, f) != 1) break; }
    return i;
}
#ifndef INK_GP_NO_REDIRECT
#define FILE ink_gp_file
#define fopen ink_gp_fopen
#define fread ink_gp_fread
#define fwrite ink_gp_fwrite
#define fseek ink_gp_fseek
#define ftell ink_gp_ftell
#define fclose ink_gp_fclose
#define feof ink_gp_feof
#define ferror ink_gp_ferror
#define fdopen(fd, mode) nullptr
#define open ink_gp_open
#define write ink_gp_write
#define lseek ink_gp_lseek
#define ftruncate ink_gp_truncate
#define close ink_gp_close
#define unlink ink_gp_unlink
#define malloc ink_gp_alloc
#define free osal_free
#define realloc(p, n) osal_realloc(p, n, OSAL_MEM_EXTERNAL)
#define strdup ink_gp_strdup
#define pthread_mutex_t osal_mutex_t *
#undef PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER nullptr
#define pthread_mutex_lock(m) ink_gp_mutex_lock(m)
#define pthread_mutex_trylock(m) ink_gp_mutex_lock(m, false)
#define pthread_mutex_unlock(m) ink_gp_mutex_unlock(m)
#define gettimeofday ink_gp_gettimeofday
#define time ink_gp_time
#define ALOGD(...) ((void)0)
#define ALOGW(...) ((void)0)
#define ALOGE(...) ((void)0)
#endif
