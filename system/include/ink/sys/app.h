/*
 * The app framework's basics (architecture v1.0 §7.3, "ink/app"): time,
 * memory, read-only content files on the card, the panel's density. Apps use
 * these and the other framework headers (ui, cap, state, settings, text,
 * font, storage, members, ai), never the HAL or the OSAL (D-48); the
 * sdk-boundary test checks the apps' sources for it.
 */
#ifndef INK_SYS_APP_H
#define INK_SYS_APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A large table of an app, zero-initialised, kept out of the scarce internal RAM (see ink/osal.h). */
#ifndef INK_PSRAM_BSS
#define INK_PSRAM_BSS
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Monotonic time (µs since boot). */
uint64_t ink_now_us(void);

/** Memory for app data: `psram` for large buffers (pages, images), else the default heap. */
void *ink_app_alloc(size_t n, bool psram);
void *ink_app_realloc(void *p, size_t n);
void ink_app_free(void *p);
/** Attribute the calling task's next allocations to `tag` in memory reports; returns the previous one. */
const char *ink_app_tag(const char *tag);
/** Bytes free now: internal RAM, PSRAM (diagnostics). */
void ink_app_mem_free(size_t *internal, size_t *internal_min, size_t *psram);
/** A cost the simulator should charge (e.g. "psram.copy_byte"); nothing on the device. */
void ink_app_cost(const char *cost_class, uint64_t units);
/** Stop with a message: an app cannot go on (out of memory at start-up). */
void ink_app_fail(const char *why) __attribute__((noreturn));

/** The panel's pixel density (dots per inch). */
int ink_panel_dpi(void);

/** Read an immutable bundled asset, or query its size with out=NULL. -1 if absent or too small. */
int64_t ink_app_asset_read(const char *name, void *out, size_t cap);

/* Read-only content files (/content/... on the card). */
typedef struct ink_file ink_file_t;
typedef struct ink_dir ink_dir_t;
int64_t ink_file_size(const char *path); /* -1 if absent */
ink_file_t *ink_file_open(const char *path);
int64_t ink_file_read(ink_file_t *f, void *buf, size_t n); /* bytes, 0 at the end, < 0 on error */
void ink_file_close(ink_file_t *f);
ink_dir_t *ink_dir_open(const char *path);
/** The next entry: 1 (name copied), 0 at the end, < 0 on error. */
int ink_dir_next(ink_dir_t *d, char *name, size_t cap, bool *is_dir);
void ink_dir_close(ink_dir_t *d);

#ifdef __cplusplus
}
#endif
#endif
