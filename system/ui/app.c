/* The app framework's basics (see ink/sys/app.h): thin, over the OSAL and the HAL. */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/sys/app.h"
#include "ink/hal_assets.h"

uint64_t ink_now_us(void) { return osal_now_us(); }

void *ink_app_alloc(size_t n, bool psram) { return osal_malloc(n, psram ? OSAL_MEM_EXTERNAL : OSAL_MEM_DEFAULT); }
void *ink_app_realloc(void *p, size_t n) { return osal_realloc(p, n, OSAL_MEM_DEFAULT); }
void ink_app_free(void *p) { osal_free(p); }
const char *ink_app_tag(const char *tag) { return osal_mem_tag(tag); }
void ink_app_cost(const char *cost_class, uint64_t units) { osal_cost_units(cost_class, units); }
void ink_app_fail(const char *why) { osal_panic("%s", why); }

void ink_app_mem_free(size_t *internal, size_t *internal_min, size_t *psram) {
    osal_mem_stats_t in, ex;
    osal_mem_stats(OSAL_MEM_INTERNAL, &in);
    osal_mem_stats(OSAL_MEM_EXTERNAL, &ex);
    if (internal) *internal = in.free;
    if (internal_min) *internal_min = in.min_free;
    if (psram) *psram = ex.free;
}

int ink_panel_dpi(void) {
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    return pi.dpi;
}

int64_t ink_file_size(const char *path) { return hal_fs_size(path); }
int64_t ink_app_asset_read(const char *name, void *out, size_t cap) {
    size_t len = 0;
    const void *data = hal_asset_map(name, &len);
    if (!data) return -1;
    bool fits = !out || cap >= len;
    if (out && fits) { hal_asset_access(data, len); memcpy(out, data, len); }
    hal_asset_unmap(data);
    return fits ? (int64_t)len : -1;
}
ink_file_t *ink_file_open(const char *path) { return (ink_file_t *)hal_fs_open(path, HAL_O_READ); }
int64_t ink_file_read(ink_file_t *f, void *buf, size_t n) { return hal_fs_read((hal_file_t *)f, buf, n); }
void ink_file_close(ink_file_t *f) { hal_fs_close((hal_file_t *)f); }
ink_dir_t *ink_dir_open(const char *path) { return (ink_dir_t *)hal_fs_opendir(path); }

int ink_dir_next(ink_dir_t *d, char *name, size_t cap, bool *is_dir) {
    hal_dirent_t e;
    int r = hal_fs_readdir((hal_dir_t *)d, &e);
    if (r == 1) {
        snprintf(name, cap, "%s", e.name);
        if (is_dir) *is_dir = e.is_dir;
    }
    return r;
}

void ink_dir_close(ink_dir_t *d) { hal_fs_closedir((hal_dir_t *)d); }
