/* Forced after apps/mogo/port/runtime.h on the lock style files only (apps/mogo_lock/mogo_lock.cmake).
 * Files: read through runtime.h, written here through ink_graft_file_open; Mogo's "/sdcard/.readpico" cache root is
 * the member's own "/me/mogo". Directories list the member's card index. time(): see lock_port.c. */
#pragma once
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
FILE *ink_mogo_lock_fopen(const char *path, const char *mode);
size_t ink_mogo_lock_fwrite(const void *p, size_t s, size_t n, FILE *f);
int ink_mogo_lock_mkdir(const char *path);
int ink_mogo_lock_rename(const char *from, const char *to);
int ink_mogo_lock_remove(const char *path);
DIR *ink_mogo_lock_opendir(const char *path);
struct dirent *ink_mogo_lock_readdir(DIR *dir);
int ink_mogo_lock_closedir(DIR *dir);
time_t ink_mogo_lock_time(time_t *out);
int ink_mogo_lock_stat(const char *path, struct stat *st);
bool ink_mogo_lock_last_path(char *out, size_t cap);
#ifndef INK_MOGO_PORT_IMPL
#undef fopen
#define fopen ink_mogo_lock_fopen
#undef fwrite
#define fwrite ink_mogo_lock_fwrite
#define mkdir(p, m) ink_mogo_lock_mkdir(p)
#define rename ink_mogo_lock_rename
#define remove ink_mogo_lock_remove
#define opendir ink_mogo_lock_opendir
#define readdir ink_mogo_lock_readdir
#define closedir ink_mogo_lock_closedir
#undef stat
#define stat(p, s) ink_mogo_lock_stat(p, s)
#define lstat(p, s) ink_mogo_lock_stat(p, s)
#undef time
#define time ink_mogo_lock_time
/* The book the ticket shows: worked out on every call, never saved (lock_port.c). (book_progress_last_path)(…) is the stored one. */
#define book_progress_last_path(out, cap) ink_mogo_lock_last_path(out, cap)
#endif
