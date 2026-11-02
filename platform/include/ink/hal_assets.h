/*
 * ink HAL — read-only assets (fonts, images, dictionaries, waveforms).
 *
 * Assets are memory-mapped, never copied into RAM:
 *   device     a flash partition with a table of contents, mapped through the
 *              MMU/cache (costs no heap; like all flash-resident data it is
 *              unreadable while internal flash is being written)
 *   POSIX      mmap() of files under the assets directory
 *   simulator  host files, accounted against the profile's assets partition
 */
#ifndef INK_HAL_ASSETS_H
#define INK_HAL_ASSETS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Map asset `name` (e.g. "fonts/sans-32.inkf"). Returns NULL if missing. */
const void *hal_asset_map(const char *name, size_t *size);
void hal_asset_unmap(const void *data);

/**
 * Hint: the caller is about to read [p, p+len) of a mapped asset. Nothing to
 * do on real hardware (the cache does the work; ports may make it an empty
 * inline); simulators charge cache misses on memory-mapped flash with it
 * (Plus: one 64-byte line over DIO 80 MHz flash ~3.6 us, and refreshes
 * stream ~1 MB of PSRAM through the same D-cache, so fonts start cold).
 */
void hal_asset_access(const void *p, size_t len);

#ifdef __cplusplus
}
#endif
#endif
