/* /user uses the storage service's existing Flash guard and audio write scheduling. */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_private/esp_cache_private.h"
#include "esp_partition.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sd_protocol_defs.h"
#include "lfs.h"
#include "ink/hal_storage.h"
#include "ink/hal_display.h"
#include "ink/osal.h"
#ifndef CONFIG_INK_SD_READONLY
#define CONFIG_INK_SD_READONLY 0
#endif
ESP_EVENT_DEFINE_BASE(INK_SD_EVENT);
static lfs_t g_lfs;
static const esp_partition_t *g_user;
static osal_mutex_t *g_lock;
static sdmmc_card_t *g_card;
static unsigned g_handles;
static bool g_mounted, g_present;
static void (*g_callback)(void *);
static void *g_context, *g_dma;
static uint8_t g_read[512], g_prog[512], g_lookahead[128];
#define LOCK() osal_mutex_lock(g_lock, OSAL_WAIT_FOREVER)
#define DONE(value) do { __auto_type result_ = (value); osal_mutex_unlock(g_lock); return result_; } while (0)
static int bd_read(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, void *p, lfs_size_t n) {
    hal_display_pause(true);
    esp_err_t r = esp_partition_read(g_user, b * c->block_size + o, p, n);
    hal_display_pause(false);
    return r == ESP_OK ? 0 : LFS_ERR_IO;
}
static int bd_prog(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, const void *p, lfs_size_t n) {
    hal_display_pause(true);
    esp_err_t r = esp_partition_write(g_user, b * c->block_size + o, p, n);
    hal_display_pause(false);
    return r == ESP_OK ? 0 : LFS_ERR_IO;
}
static int bd_erase(const struct lfs_config *c, lfs_block_t b) {
    hal_display_pause(true);
    esp_err_t r = esp_partition_erase_range(g_user, b * c->block_size, c->block_size);
    hal_display_pause(false);
    return r == ESP_OK ? 0 : LFS_ERR_IO;
}
static int bd_sync(const struct lfs_config *c) { (void)c; return 0; }
static const struct lfs_config g_cfg = {.read = bd_read, .prog = bd_prog, .erase = bd_erase, .sync = bd_sync,
    .read_size = 128, .prog_size = 128, .block_size = 4096, .block_count = 100, .block_cycles = 512,
    .cache_size = 512, .lookahead_size = 128, .read_buffer = g_read, .prog_buffer = g_prog, .lookahead_buffer = g_lookahead};
static bool dma_ok(int slot, const void *p, size_t n) {
    return esp_ptr_internal(p) && sdmmc_host_check_buffer_alignment(slot, p, n);
}
static esp_err_t transaction(int slot, sdmmc_command_t *cmd) {
    if ((CONFIG_INK_SD_READONLY || !__atomic_load_n(&g_present, __ATOMIC_ACQUIRE)) &&
        (cmd->opcode == MMC_WRITE_BLOCK_SINGLE || cmd->opcode == MMC_WRITE_BLOCK_MULTIPLE || cmd->opcode == MMC_ERASE))
        return ESP_ERR_NOT_SUPPORTED;
    return sdmmc_host_do_transaction(slot, cmd);
}
static void card_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)base; (void)id; (void)data;
    LOCK();
    bool was = g_present;
    if (g_card && sdmmc_get_status(g_card) != ESP_OK) __atomic_store_n(&g_present, false, __ATOMIC_RELEASE);
    if (g_card && !g_present && !g_handles && esp_vfs_fat_sdcard_unmount("/sd", g_card) == ESP_OK) g_card = NULL;
    if (!g_card) {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.slot = 0;
        host.check_buffer_alignment = dma_ok;
        host.do_transaction = transaction;
        host.dma_aligned_buffer = g_dma;
        host.unaligned_multi_block_rw_max_chunk_size = 8;
        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.clk = 24; slot.cmd = 25; slot.d0 = 20; slot.d1 = 21; slot.d2 = 22; slot.d3 = 23;
        slot.width = 4; slot.flags = 0;
        const int pins[] = {24, 25, 20, 21, 22, 23};
        for (unsigned i = 0; i < sizeof pins / sizeof pins[0]; i++) gpio_hold_dis(pins[i]);
        esp_vfs_fat_mount_config_t cfg = {.max_files = 12, .allocation_unit_size = 16384, .disk_status_check_enable = true};
        if (g_dma && esp_vfs_fat_sdmmc_mount("/sd", &host, &slot, &cfg, &g_card) == ESP_OK)
            __atomic_store_n(&g_present, true, __ATOMIC_RELEASE);
    }
    if (!was && g_present && !CONFIG_INK_SD_READONLY) {
        mkdir("/sd/content", 0755);
        mkdir("/sd/cache", 0755);
    }
    void (*cb)(void *) = g_callback;
    void *ctx = g_context;
    bool changed = was != g_present;
    osal_mutex_unlock(g_lock);
    if (changed && cb) cb(ctx);
}
static void card_tick(void *arg) { (void)arg; esp_event_post(INK_SD_EVENT, 0, NULL, 0, 0); }
void ink_s31_storage_init(void) {
    g_lock = osal_mutex_create();
    g_user = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "model");
    if (g_user && g_user->address == 0x10000 && g_user->size == 400 * 1024) {
        int r = lfs_mount(&g_lfs, &g_cfg);
        if (r == LFS_ERR_CORRUPT && !lfs_format(&g_lfs, &g_cfg)) r = lfs_mount(&g_lfs, &g_cfg);
        g_mounted = r == 0;
    }
    size_t alignment = 0;
    if (esp_cache_get_alignment(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA, &alignment) == ESP_OK)
        g_dma = heap_caps_aligned_alloc(alignment < 4 ? 4 : alignment, 4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    esp_err_t e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return;
    if (esp_event_handler_register(INK_SD_EVENT, 0, card_event, NULL) != ESP_OK) return;
    esp_timer_handle_t timer;
    esp_timer_create_args_t cfg = {.callback = card_tick, .name = "card"};
    if (esp_timer_create(&cfg, &timer) == ESP_OK) esp_timer_start_periodic(timer, 1000000);
    card_tick(NULL);
}
static int route(const char *path, char out[512]) {
    if (!path || strstr(path, "..")) return -1;
    if (!strncmp(path, "/user", 5) && (!path[5] || path[5] == '/')) {
        if (!g_mounted || strlen(path + 5) >= 512) return -1;
        strcpy(out, path[5] ? path + 5 : "/");
        return 0;
    }
    if (!g_present || !((!strncmp(path, "/content", 8) && (!path[8] || path[8] == '/')) ||
                       (!strncmp(path, "/cache", 6) && (!path[6] || path[6] == '/')))) return -1;
    return snprintf(out, 512, "/sd%s", path) < 512 ? 1 : -1;
}
struct hal_file { int vol, fd; lfs_file_t file; struct lfs_file_config cfg; uint8_t cache[512]; };
struct hal_dir { int vol; lfs_dir_t dir; DIR *fat; char path[512]; };
hal_file_t *hal_fs_open(const char *path, int flags) {
    LOCK();
    char p[512]; int v = route(path, p);
    if (v < 0 || (v && CONFIG_INK_SD_READONLY && flags != HAL_O_READ)) DONE((hal_file_t *)NULL);
    hal_file_t *f = heap_caps_calloc(1, sizeof *f, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!f) DONE((hal_file_t *)NULL);
    f->vol = v; f->cfg.buffer = f->cache;
    int mode = flags & HAL_O_WRITE ? (flags & HAL_O_READ ? LFS_O_RDWR : LFS_O_WRONLY) : LFS_O_RDONLY;
    int posix = flags & HAL_O_WRITE ? (flags & HAL_O_READ ? O_RDWR : O_WRONLY) : O_RDONLY;
    if (flags & HAL_O_CREATE) { mode |= LFS_O_CREAT; posix |= O_CREAT; }
    if (flags & HAL_O_TRUNC) { mode |= LFS_O_TRUNC; posix |= O_TRUNC; }
    if (flags & HAL_O_APPEND) { mode |= LFS_O_APPEND; posix |= O_APPEND; }
    int r = v ? (f->fd = open(p, posix, 0644)) : lfs_file_opencfg(&g_lfs, &f->file, p, mode, &f->cfg);
    if (r < 0) { heap_caps_free(f); DONE((hal_file_t *)NULL); }
    if (v) g_handles++;
    DONE(f);
}
ssize_t hal_fs_read(hal_file_t *f, void *p, size_t n) {
    LOCK();
    DONE(!f ? -1 : !f->vol ? lfs_file_read(&g_lfs, &f->file, p, n) : g_present ? read(f->fd, p, n) : -1);
}
ssize_t hal_fs_write(hal_file_t *f, const void *p, size_t n) {
    LOCK();
    DONE(!f ? -1 : !f->vol ? lfs_file_write(&g_lfs, &f->file, p, n) : g_present && !CONFIG_INK_SD_READONLY ? write(f->fd, p, n) : -1);
}
int hal_fs_seek(hal_file_t *f, int64_t off, int whence) {
    LOCK();
    int64_t r = !f ? -1 : !f->vol ? lfs_file_seek(&g_lfs, &f->file, off, whence) : g_present ? lseek(f->fd, off, whence) : -1;
    DONE(r < 0 ? -1 : 0);
}
int64_t hal_fs_tell(hal_file_t *f) { LOCK(); DONE(!f ? -1 : !f->vol ? lfs_file_tell(&g_lfs, &f->file) : g_present ? lseek(f->fd, 0, SEEK_CUR) : -1); }
int hal_fs_sync(hal_file_t *f) { LOCK(); DONE(!f ? -1 : !f->vol ? lfs_file_sync(&g_lfs, &f->file) : g_present ? fsync(f->fd) : -1); }
int hal_fs_close(hal_file_t *f) {
    LOCK();
    if (!f) DONE(-1);
    int r = f->vol ? close(f->fd) : lfs_file_close(&g_lfs, &f->file);
    if (f->vol) g_handles--;
    heap_caps_free(f); DONE(r);
}
int hal_fs_remove(const char *path) {
    LOCK(); char p[512]; int v = route(path, p);
    DONE(v < 0 ? -1 : !v ? lfs_remove(&g_lfs, p) : CONFIG_INK_SD_READONLY ? -1 : remove(p));
}
int hal_fs_mkdir(const char *path) {
    LOCK(); char p[512]; int v = route(path, p);
    DONE(v < 0 ? -1 : !v ? lfs_mkdir(&g_lfs, p) : CONFIG_INK_SD_READONLY ? -1 : mkdir(p, 0755));
}
int hal_fs_rename(const char *from, const char *to) {
    LOCK(); char a[512], b[512]; int v = route(from, a), w = route(to, b);
    DONE(v < 0 || v != w ? -1 : !v ? lfs_rename(&g_lfs, a, b) : CONFIG_INK_SD_READONLY ? -1 : rename(a, b));
}
static int info(const char *path, int64_t *size, int64_t *mtime) {
    char p[512]; int v = route(path, p);
    struct lfs_info li; struct stat st;
    if (v < 0) return -1;
    int r = v ? stat(p, &st) : lfs_stat(&g_lfs, p, &li);
    if (r < 0) return (v ? errno == ENOENT : r == LFS_ERR_NOENT) ? 0 : -1;
    *size = v ? (int64_t)st.st_size : (int64_t)li.size; *mtime = v ? (int64_t)st.st_mtime * 1000000000 : -1;
    return 1;
}
int hal_fs_exists(const char *path) { LOCK(); int64_t n, t; DONE(info(path, &n, &t)); }
int64_t hal_fs_size(const char *path) { LOCK(); int64_t n, t; int r = info(path, &n, &t); DONE(r == 1 ? n : -1); }
int64_t hal_fs_mtime(const char *path) { LOCK(); int64_t n, t; int r = info(path, &n, &t); DONE(r == 1 ? t : -1); }
hal_dir_t *hal_fs_opendir(const char *path) {
    LOCK(); char p[512]; int v = route(path, p);
    if (v < 0) DONE((hal_dir_t *)NULL);
    hal_dir_t *d = heap_caps_calloc(1, sizeof *d, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!d) DONE((hal_dir_t *)NULL);
    d->vol = v; strcpy(d->path, p);
    int r = v ? ((d->fat = opendir(p)) ? 0 : -1) : lfs_dir_open(&g_lfs, &d->dir, p);
    if (r < 0) { heap_caps_free(d); DONE((hal_dir_t *)NULL); }
    if (v) g_handles++;
    DONE(d);
}
int hal_fs_readdir(hal_dir_t *d, hal_dirent_t *out) {
    LOCK();
    if (!d) DONE(-1);
    for (;;) {
        struct lfs_info li;
        if (!d->vol) {
            int r = lfs_dir_read(&g_lfs, &d->dir, &li);
            if (r <= 0) DONE(r);
            *out = (hal_dirent_t){.is_dir = li.type == LFS_TYPE_DIR, .size = li.size, .mtime = -1};
            strcpy(out->name, li.name);
        } else {
            if (!g_present) DONE(-1);
            errno = 0; struct dirent *e = readdir(d->fat);
            if (!e) DONE(errno ? -1 : 0);
            char p[768]; struct stat st;
            int n = snprintf(p, sizeof p, "%s/%s", d->path, e->d_name);
            if (n < 0 || (size_t)n >= sizeof p || stat(p, &st)) DONE(-1);
            *out = (hal_dirent_t){.is_dir = S_ISDIR(st.st_mode), .size = st.st_size, .mtime = (int64_t)st.st_mtime * 1000000000};
            snprintf(out->name, sizeof out->name, "%s", e->d_name);
        }
        if (strcmp(out->name, ".") && strcmp(out->name, "..")) DONE(1);
    }
}
void hal_fs_closedir(hal_dir_t *d) {
    LOCK();
    if (d) {
        if (d->vol) { closedir(d->fat); g_handles--; }
        else lfs_dir_close(&g_lfs, &d->dir);
        heap_caps_free(d);
    }
    osal_mutex_unlock(g_lock);
}
int hal_fs_volume_info(const char *volume, hal_fs_volume_info_t *out) {
    LOCK();
    if (!strcmp(volume, "/user") && g_mounted) {
        lfs_ssize_t n = lfs_fs_size(&g_lfs);
        if (n < 0) DONE(-1);
        *out = (hal_fs_volume_info_t){409600, (100 - (uint64_t)n) * 4096}; DONE(0);
    }
    DONE(g_present && (!strcmp(volume, "/content") || !strcmp(volume, "/cache")) &&
         esp_vfs_fat_info("/sd", &out->total_bytes, &out->free_bytes) == ESP_OK ? 0 : -1);
}
bool hal_fs_card_present(void) { return __atomic_load_n(&g_present, __ATOMIC_ACQUIRE); }
void hal_fs_set_card_callback(void (*cb)(void *), void *ctx) { LOCK(); g_callback = cb; g_context = ctx; osal_mutex_unlock(g_lock); }
void hal_fs_new_card_id(uint8_t out[16]) { esp_fill_random(out, 16); }
