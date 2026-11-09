/*
 * ink HAL — storage.
 *
 * The system sees three logical volumes with explicit durability classes,
 * independent of where they physically live:
 *
 *   /user     small, power-loss-safe user state (device: LittleFS on internal
 *             flash; Linux: ext4 directory with fsync). Writes are EXPENSIVE on
 *             the MCU (cache disabled, PSRAM stalls) — batch them.
 *   /content  bulk, re-downloadable content: books, audio (device: FAT on TF card)
 *   /cache    rebuildable caches: page layouts, glyph caches (may be wiped)
 *
 * The API is a deliberately small POSIX-like subset so it maps onto ESP-IDF
 * VFS, Linux, and the simulator's flash/SD models.
 */
#ifndef INK_HAL_STORAGE_H
#define INK_HAL_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_file hal_file_t;

enum {
    HAL_O_READ = 1,
    HAL_O_WRITE = 2,
    HAL_O_CREATE = 4,
    HAL_O_TRUNC = 8,
    HAL_O_APPEND = 16,
};

hal_file_t *hal_fs_open(const char *path, int flags);
ssize_t hal_fs_read(hal_file_t *f, void *buf, size_t len);
ssize_t hal_fs_write(hal_file_t *f, const void *buf, size_t len);
int hal_fs_seek(hal_file_t *f, int64_t off, int whence); /* SEEK_SET/CUR/END */
int64_t hal_fs_tell(hal_file_t *f);
/** Make written data durable (power-loss safe once this returns 0). */
int hal_fs_sync(hal_file_t *f);
int hal_fs_close(hal_file_t *f); /* implies sync on /user */
int hal_fs_remove(const char *path);
/** Atomic replace (used for "write temp file, then rename"). */
int hal_fs_rename(const char *from, const char *to);
int hal_fs_mkdir(const char *path);
int64_t hal_fs_size(const char *path); /* -1 if missing */
int64_t hal_fs_mtime(const char *path); /* card metadata, nanoseconds; -1 on error */

typedef struct hal_dir hal_dir_t;
typedef struct {
    char name[256];
    int is_dir;
    int64_t size;
} hal_dirent_t;

hal_dir_t *hal_fs_opendir(const char *path);
/** Returns 1 and fills `out` for each entry, 0 at the end, <0 on error. */
int hal_fs_readdir(hal_dir_t *d, hal_dirent_t *out);
void hal_fs_closedir(hal_dir_t *d);

typedef struct {
    uint64_t total_bytes;
    uint64_t free_bytes;
} hal_fs_volume_info_t;

int hal_fs_volume_info(const char *volume /* "/user" ... */, hal_fs_volume_info_t *out);
/** The card is in and mounted: /content and /cache work (they fail otherwise). Does not wait; whether a
 *  card pulled out is noticed before the next access depends on the board (no card-detect line known). */
bool hal_fs_card_present(void);

#ifdef __cplusplus
}
#endif
#endif
