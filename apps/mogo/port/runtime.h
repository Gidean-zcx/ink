#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include "ink/sys/i18n.h"
#include "ink/sys/app.h"
#include <graft.h>
time_t ink_mogo_time(time_t *out);
void ink_mogo_qsort(void *,size_t,size_t,int (*)(const void *,const void *));
void *ink_mogo_malloc(size_t n);
void *ink_mogo_calloc(size_t n, size_t s);
FILE *ink_mogo_fopen(const char *path, const char *mode);
size_t ink_mogo_fread(void *p, size_t s, size_t n, FILE *f);
size_t ink_mogo_fwrite(const void *p, size_t s, size_t n, FILE *f);
int ink_mogo_fclose(FILE *f);
int ink_mogo_ferror(FILE *f);
void ink_mogo_clearerr(FILE *f);
int ink_mogo_fgetc(FILE *f);
int ink_mogo_fseek(FILE *f, long off, int whence);
long ink_mogo_ftell(FILE *f);
int ink_mogo_stat(const char *path, struct stat *st);
int ink_mogo_open(const char *path, int flags, ...);
int ink_mogo_close(int fd);
ssize_t ink_mogo_read(int fd, void *p, size_t n);
off_t ink_mogo_lseek(int fd, off_t off, int whence);
int ink_mogo_snprintf(char *out, size_t cap, const char *fmt, ...);
void ink_mogo_owner(const ink_member_identity_t *owner);
const ink_member_identity_t *ink_mogo_current_owner(void);
void ink_mogo_ui_coordinates(bool enabled);
void ink_mogo_canvas(int w, int h);
void ink_mogo_dimensions(int w, int h, int px);
#ifndef INK_MOGO_PORT_IMPL
#define time ink_mogo_time
#define qsort ink_mogo_qsort
#define malloc ink_mogo_malloc
#define calloc ink_mogo_calloc
#define realloc ink_app_realloc
#define free ink_app_free
#define fopen ink_mogo_fopen
#define fread ink_mogo_fread
#define fwrite ink_mogo_fwrite
#define fclose ink_mogo_fclose
#define ferror ink_mogo_ferror
#define clearerr ink_mogo_clearerr
#define fgetc ink_mogo_fgetc
#define fseek ink_mogo_fseek
#define ftell ink_mogo_ftell
#define stat(p,s) ink_mogo_stat(p,s)
#define open ink_mogo_open
#define close ink_mogo_close
#define read ink_mogo_read
#define lseek ink_mogo_lseek
#undef snprintf
#define snprintf ink_mogo_snprintf
#endif
