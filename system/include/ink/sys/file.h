/*
 * File service (R-052, R-058; system services design §4): files on the card,
 * in one set of paths that say whose they are, whatever task asks.
 *
 *   /me/...               the current member's files
 *   /family/...           family content: everyone reads, adults write
 *   /members/<id>/...     another member's files: adults only
 *   /app/<app>/...        an app's own files of the current member (not for the AI)
 *   /cache/<owner>/...    rebuildable (not for the AI)
 *
 * Physical paths and /user are not reachable from here at all. A card access
 * blocks its caller for milliseconds up to hundreds of milliseconds (flash
 * writes stop every task, card writes do not): keep long ones off the UI task.
 * Long copies and deletes run as jobs.
 */
#ifndef INK_SYS_FILE_H
#define INK_SYS_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INK_FS_OK = 0,
    INK_FS_ERR_PATH = -1,      /* not a valid path */
    INK_FS_ERR_DENIED = -2,    /* not for this member */
    INK_FS_ERR_NOT_FOUND = -3,
    INK_FS_ERR_EXISTS = -4,
    INK_FS_ERR_NO_CARD = -5,
    INK_FS_ERR_USB = -6,       /* the card is lent to a computer */
    INK_FS_ERR_NO_SPACE = -7,
    INK_FS_ERR_IO = -8,
    INK_FS_ERR_BUSY = -9,      /* too many files open, a job already runs, files in use */
    INK_FS_ERR_IS_DIR = -10,
    INK_FS_ERR_NO_HOST = -11,  /* no computer at the other end of the cable */
} ink_fs_err_t;

/** A short word for people and the model. */
const char *ink_fs_err_name(int err);

enum { INK_FS_READ = 1, INK_FS_WRITE = 2, INK_FS_APPEND = 4, INK_FS_CREATE = 8, INK_FS_EXCL = 16 };

#define INK_FS_PATH_MAX 128
#define INK_FS_NAME_MAX 64

typedef struct ink_fs_file ink_fs_file_t;
typedef struct ink_fs_dir ink_fs_dir_t;

typedef struct {
    char name[INK_FS_NAME_MAX];
    bool is_dir;
    int64_t size;
} ink_fs_stat_t;

int ink_file_service_init(void);
/** Refresh the state items that say how the card is (blocks on the card: not for the UI task). */
void ink_fs_refresh_state(void);

/** 0 and *out, or an error. Up to 8 files at a time. */
int ink_fs_open(const char *path, int mode, ink_fs_file_t **out);
/** Trusted delayed callers keep the original actor, permission and incarnation. */
int ink_fs_open_member_generation(const char *member, uint32_t generation, const char *path, int mode, ink_fs_file_t **out);
int64_t ink_fs_read(ink_fs_file_t *f, void *buf, size_t n);
int64_t ink_fs_write(ink_fs_file_t *f, const void *buf, size_t n);
int ink_fs_seek(ink_fs_file_t *f, int64_t off, int whence);
int64_t ink_fs_tell(ink_fs_file_t *f);
int ink_fs_sync(ink_fs_file_t *f);
int ink_fs_close(ink_fs_file_t *f);
uint32_t ink_fs_file_revision(ink_fs_file_t *f);
/** Conditional inverses share the file mutation lock. Any later card mutation invalidates them. */
int ink_fs_remove_if_revision(const char *path, uint32_t revision);
int ink_fs_move_if_revision(const char *from, const char *to, uint32_t revision, uint32_t *written);

int ink_fs_stat(const char *path, ink_fs_stat_t *out);
int ink_fs_dir_open(const char *path, ink_fs_dir_t **out);
/** The next entry: 1, 0 at the end, < 0 on an error. Hidden and unfinished files do not appear. */
int ink_fs_dir_next(ink_fs_dir_t *d, ink_fs_stat_t *out);
void ink_fs_dir_close(ink_fs_dir_t *d);
/** Makes the missing parents too. An existing directory is fine. */
int ink_fs_mkdir(const char *path);
/** Delayed service work retains the member that requested the directory. */
int ink_fs_mkdir_member_generation(const char *member, uint32_t generation, const char *path);
/** A file or an empty directory. Whole trees: ink_fs_delete_start. */
int ink_fs_remove(const char *path);
int ink_fs_remove_member_generation(const char *member, uint32_t generation, const char *path);
/** On the card that is a rename: at once. The target must not exist. */
int ink_fs_move(const char *from, const char *to);
/** Replace or create a whole file through a temporary one. */
int ink_fs_write_atomic(const char *path, const void *data, size_t n);
/** Backup service only: install this member's .backup/down<stage> at a /me path, under the edit lock.
 *  Checks the previous hash (zero = absent), rejects open writers/jobs and changed originals. */
int ink_fs_restore(const char *member, uint32_t generation, unsigned stage, const char *path, const uint8_t expected_sha256[32]);
/** Trusted backup continuation: after hashing outside the edit lock, install only if the card revision
 *  still matches. Open writers and jobs also refuse installation; a busy edit lock returns BUSY.
 *  Revision sampling never waits: zero means an edit, writer, job or USB is active; retry later.
 *  Sample before reading metadata/content, then verify the same nonzero revision afterwards. */
uint32_t ink_fs_revision(void);
int ink_fs_restore_if_revision(const char *member, uint32_t generation, unsigned stage, const char *path, uint32_t revision);
int ink_fs_free(uint64_t *free_bytes, uint64_t *total_bytes);
/** A member was removed: their folder, which nobody can delete by its path. Blocks on the card. INK_FS_OK, or why not. */
int ink_fs_forget_member(const char *id);
/** Factory reset, 0 or why not (no card, lent): every member's files, what is downloaded and the cache. The family's shared content stays. */
int ink_fs_wipe_personal(void);

/* Jobs: files and trees that may take minutes, one at a time, on the background executor. */
typedef enum { INK_FS_JOB_NONE = 0, INK_FS_JOB_RUNNING, INK_FS_JOB_DONE, INK_FS_JOB_FAILED, INK_FS_JOB_CANCELLED } ink_fs_job_state_t;
typedef struct {
    ink_fs_job_state_t state;
    uint64_t bytes_done, bytes_total;
    uint32_t files_done, files_total;
    int err;
} ink_fs_job_status_t;
/** Copy a file or a tree. The target must not exist and must fit. Its id (> 0), or an error. Told by the event
 *  file.job_done. A file being copied is complete or not there: it is written as <name>.part first. */
int ink_fs_copy_start(const char *from, const char *to);
/** Delete a file or a tree. Its id, or an error. */
int ink_fs_delete_start(const char *path);
int ink_fs_job_status(int id, ink_fs_job_status_t *out);
void ink_fs_job_cancel(int id);
/** fn(ctx, status) once when the job ends, on the background executor; for callers on that executor right
 *  after starting it (nothing can end in between). */
void ink_fs_job_on_end(int id, void (*fn)(void *ctx, const ink_fs_job_status_t *st), void *ctx);

/*
 * The card lent to a computer as a USB disk (R-058). Only the card: the system area and /user never.
 * It takes all the files: before lending, holders are told and get a moment to close theirs.
 */
typedef void (*ink_fs_release_fn)(void *ctx); /* close your files now (any task, short). Up to 4. */
int ink_fs_on_release(ink_fs_release_fn fn, void *ctx);
/** Lend the card: 0, or INK_FS_ERR_BUSY (files in use, a job runs), INK_FS_ERR_NO_CARD, INK_FS_ERR_NO_HOST. Blocks up to 3 s. */
int ink_usb_disk_start(void);
/** Take it back. 0, or INK_FS_ERR_NO_CARD when it does not mount again. */
int ink_usb_disk_stop(void);
bool ink_usb_disk_active(void);
/** For the system's own folders on the card, which do not go through a path here: while it is held the card is not lent.
 *  INK_FS_OK (and then ink_fs_card_release), or INK_FS_ERR_USB, INK_FS_ERR_NO_CARD. Hold it no longer than a file stays open. */
int ink_fs_card_hold(void);
void ink_fs_card_release(void);

#ifdef __cplusplus
}
#endif
#endif
