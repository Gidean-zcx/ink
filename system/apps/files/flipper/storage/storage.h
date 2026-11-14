/* Shim: the two favourites "files" live in files.prefs (files_flipper.c); real paths go to the file service. */
#pragma once
#include <furi.h>
#define STORAGE_EXT_PATH_PREFIX "/ext"
#define EXT_PATH(p) STORAGE_EXT_PATH_PREFIX "/" p
typedef struct Storage Storage;
typedef struct File File;
typedef struct { uint32_t flags; uint64_t size; } FileInfo;
typedef enum { FSE_OK, FSE_NOT_READY, FSE_EXIST, FSE_NOT_EXIST, FSE_INVALID_PARAMETER, FSE_DENIED, FSE_INTERNAL } FS_Error;
typedef enum { FSAM_READ = 1, FSAM_WRITE = 2 } FS_AccessMode;
typedef enum { FSOM_OPEN_EXISTING = 1, FSOM_OPEN_ALWAYS, FSOM_OPEN_APPEND, FSOM_CREATE_NEW, FSOM_CREATE_ALWAYS } FS_OpenMode;
File *storage_file_alloc(Storage *s);
void storage_file_free(File *f);
bool storage_file_open(File *f, const char *path, FS_AccessMode access, FS_OpenMode mode);
bool storage_file_close(File *f);
size_t storage_file_read(File *f, void *buf, size_t n);
size_t storage_file_write(File *f, const void *buf, size_t n);
uint64_t storage_file_tell(File *f);
bool storage_file_seek(File *f, uint32_t offset, bool from_start);
FS_Error storage_file_get_error(File *f);
bool storage_file_exists(Storage *s, const char *path);
bool storage_common_exists(Storage *s, const char *path);
FS_Error storage_common_remove(Storage *s, const char *path);
FS_Error storage_common_rename(Storage *s, const char *from, const char *to);
FS_Error storage_common_stat(Storage *s, const char *path, FileInfo *info);
bool storage_simply_remove_recursive(Storage *s, const char *path);
#define file_info_is_dir(info) ((info)->flags != 0)
