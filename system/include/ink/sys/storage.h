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
/** Read a small file into buf (blocks while the storage task reads it); bytes read or -1. Sees queued writes. */
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

#ifdef __cplusplus
}
#endif
#endif
