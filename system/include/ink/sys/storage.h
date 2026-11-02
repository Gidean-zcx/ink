/*
 * Storage service: the only code that touches /user (internal flash), for
 * durable, power-safe small user state, without hurting the display or audio.
 *
 * On the MCU every flash API call — reads included — disables the caches: no
 * task runs on either core meanwhile, and a task whose stack is in PSRAM
 * would crash. So /user is accessed only from the storage task (internal
 * stack): reads are executed there for the caller, writes are queued, wait
 * for the panel to be idle and for audio to allow it, and hold a "flash
 * guard" during which the display starts no refresh (upstream: EpdFlashGuard).
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
/** Queue an atomic replace of `path` (/user/...) with a copy of data. Coalesces repeated writes of one path. */
int ink_storage_write_atomic(const char *path, const void *data, size_t len);
/** Read a small file into buf (blocks while the storage task reads it); bytes read or -1. Sees queued writes. */
int ink_storage_read(const char *path, void *buf, size_t cap);
/**
 * Hold queued writes off (reference counted), e.g. while audio streams: its
 * DMA ring bridges only ~60 ms of frozen tasks. Writes held longer than 30 s
 * are forced (counted as storage.forced_writes).
 */
void ink_storage_hold_writes(bool hold);
/** Writes waiting in the queue (0 = all durable). */
int ink_storage_pending(void);

#ifdef __cplusplus
}
#endif
#endif
