/*
 * Storage service: the only code that touches /user (internal flash), for
 * durable, power-safe small user state, without hurting the display or audio.
 *
 * On the MCU every flash API call — reads included — disables the caches: no
 * task runs on either core meanwhile, and a task whose stack is in PSRAM
 * would crash. So /user is accessed only from the storage task (internal
 * stack): reads are executed there for the caller, writes are queued, wait
 * for the panel to be idle and for audio to allow it. Every flash operation
 * holds a "flash guard": it waits for the waveform in flight and no refresh
 * starts until the flash is done (upstream: EpdFlashGuard, for writes only).
 * Small files are kept in a read cache (external RAM), filled on reads,
 * writes and prefetches, so reads during a refresh rarely wait.
 * Writes are atomic: temp file + rename (foundation §3, §5.5.1).
 */
#ifndef INK_SYS_STORAGE_H
#define INK_SYS_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ink_storage_init(void);
/** Offer the read cache to the resource manager (after ink_res_init). */
int ink_storage_reclaimable(void);
/** Queue an atomic replace of `path` (/user/...) with a copy of data. Coalesces repeated writes of one path. */
int ink_storage_write_atomic(const char *path, const void *data, size_t len);
/** Read a small file into buf (blocks while the storage task reads it); bytes read or -1. Sees queued
 *  replaces and removes. */
int ink_storage_read(const char *path, void *buf, size_t cap);
/** Load a small file into the read cache in the background, when the panel is idle (e.g. the
 *  progress files of the books on a shelf), so a later read needs no flash access. */
void ink_storage_prefetch(const char *path);
/**
 * Hold queued writes off (reference counted), e.g. while audio streams: its
 * DMA ring bridges only ~60 ms of frozen tasks. Writes held longer than 30 s
 * are forced (counted as storage.forced_writes).
 */
void ink_storage_hold_writes(bool hold);
/** Writes waiting in the queue, including the one being written (0 = nothing left to write). */
int ink_storage_pending(void);
/**
 * Writes that failed since boot (e.g. /user full). A failed write leaves the previous content of
 * its path in place, and reads of that path return the flash content again. Queued (accepted) is
 * not stored: a caller that must know waits for pending() == 0 and compares failed().
 */
int ink_storage_failed(void);

/*
 * Work with a result (system services design §3.1). Jobs run in the order they were accepted; `done`
 * runs in the storage task when one has finished (result 0, or < 0): keep it short. `data` is
 * borrowed: it stays valid until `done`. Directories a path needs are made when it is first
 * written. -1: the queue is full (8), nothing happened and `done` will not run: try again later.
 */
typedef void (*ink_storage_done_fn)(void *ctx, int result);
/** Atomic replace, without a copy, told when it is on the flash (not merged with other writes). */
int ink_storage_write_borrow(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx);
/** Append to a file, creating it. All of it or nothing: a failure leaves the file as it was. Reads do not
 *  see an append until it is written: the data store keeps what it appends in memory. */
int ink_storage_append(const char *path, const void *data, size_t len, ink_storage_done_fn done, void *ctx);
/** Remove a file; one that does not exist counts as removed. Reads see it gone at once. `done` may be NULL. */
int ink_storage_remove(const char *path, ink_storage_done_fn done, void *ctx);
/** Size of a file (blocks), -1 if it does not exist. Flash content: queued jobs do not count. */
int64_t ink_storage_size(const char *path);
/** A whole file into a new external-RAM block (ink_storage_free it): read in small pieces, the panel may
 *  refresh between them. 0 and *out set (NULL for an empty file), INK_STORAGE_NO_FILE, or INK_STORAGE_LOAD_FAILED: there is
 *  a file and it could not be had (no memory, a read error): what it holds must not be taken for nothing. */
enum { INK_STORAGE_NO_FILE = -1, INK_STORAGE_LOAD_FAILED = -2 };
int ink_storage_load(const char *path, void **out, size_t *len);
void ink_storage_free(void *block);
/** Bytes on the /user volume (blocks; walks the file system: slow, not often). 0 or -1. */
int ink_storage_volume(uint64_t *total_bytes, uint64_t *free_bytes);
/** In the storage task only (ink_storage_run): every file and folder on /user is gone, except the files named in keep (ended by
 *  NULL). False: something is still there. */
bool ink_storage_wipe_user(const char *const *keep);
/** Run fn(arg) in the storage task (blocks until it finished), for code that must touch the flash:
 *  with `guarded` the flash guard is held around it. Keep it short: every other flash user waits. */
void ink_storage_run(void (*fn)(void *arg), void *arg, bool guarded);

#ifdef __cplusplus
}
#endif
#endif
