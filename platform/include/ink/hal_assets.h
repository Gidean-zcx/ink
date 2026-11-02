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

#ifdef __cplusplus
}
#endif
#endif
