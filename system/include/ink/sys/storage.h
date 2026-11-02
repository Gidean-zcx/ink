/*
 * Storage service: durable, power-safe writes of small user state without
 * hurting the display.
 *
 * On the MCU every internal-flash erase/program disables the caches: PSRAM
 * and flash-resident code freeze on both cores, and an e-ink waveform that is
 * playing underruns (visible glitch). So writes to /user are queued, wait for
 * the panel to be idle, and hold a "flash guard" during which the display
 * service starts no refresh (upstream: EpdFlashGuard). Writes are atomic:
 * temp file + rename (foundation §3, §5.5.1).
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
/** Read a whole small file into buf; returns bytes read or -1. */
int ink_storage_read(const char *path, void *buf, size_t cap);
/** Writes waiting in the queue (0 = all durable). */
int ink_storage_pending(void);

#ifdef __cplusplus
}
#endif
#endif
