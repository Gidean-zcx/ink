/* Execute the real HAL and vendored LittleFS; only IDF and the card are fakes. */
#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "storage_idf.h"
#include "ink/osal.h"
static char g_root[512];
static const char *physical(const char *p, char out[1024]) {
    assert(!strncmp(p, "/sd", 3));
    int n = snprintf(out, 1024, "%s%s", g_root, p + 3);
    assert(n > 0 && n < 1024);
    return out;
}
static int fake_open(const char *p, int flags, int mode) { char s[1024]; return open(physical(p, s), flags, mode); }
static int fake_remove(const char *p) { char s[1024]; return remove(physical(p, s)); }
static int fake_mkdir(const char *p, int mode) { char s[1024]; return mkdir(physical(p, s), mode); }
static int fake_stat(const char *p, struct stat *st) { char s[1024]; return stat(physical(p, s), st); }
static int fake_rename(const char *a, const char *b) { char x[1024], y[1024]; return rename(physical(a, x), physical(b, y)); }
static DIR *fake_opendir(const char *p) { char s[1024]; return opendir(physical(p, s)); }
#define open(p, f, m) fake_open(p, f, m)
#define remove(p) fake_remove(p)
#define mkdir(p, m) fake_mkdir(p, m)
#define stat(p, s) fake_stat(p, s)
#define rename(a, b) fake_rename(a, b)
#define opendir(p) fake_opendir(p)
#include "../hal_storage_s31.c"
#undef open
#undef remove
#undef mkdir
#undef stat
#undef rename
#undef opendir

static uint8_t flash[400 * 1024];
static const esp_partition_t user = {0x10000, sizeof flash};
static sdmmc_card_t card;
static bool inserted;
static unsigned erases, callbacks, transactions;
static test_event_fn handler;
static bool pending;
static unsigned lock_depth;
static bool fail_read;
static bool display_paused;
void hal_display_pause(bool pause) { assert(display_paused != pause); display_paused = pause; }
struct osal_mutex { int unused; };
static struct osal_mutex mutex;
osal_mutex_t *osal_mutex_create(void) { return &mutex; }
osal_err_t osal_mutex_lock(osal_mutex_t *m, int64_t timeout) { (void)m; (void)timeout; assert(lock_depth++ == 0); return OSAL_OK; }
void osal_mutex_unlock(osal_mutex_t *m) { (void)m; assert(lock_depth-- == 1); }
void *heap_caps_calloc(size_t n, size_t size, unsigned caps) { assert(caps & MALLOC_CAP_INTERNAL); return calloc(n, size); }
void *heap_caps_aligned_alloc(size_t a, size_t n, unsigned caps) { assert(a == 128 && (caps & MALLOC_CAP_INTERNAL)); return malloc(n); }
void heap_caps_free(void *p) { free(p); }
const esp_partition_t *esp_partition_find_first(int t, int s, const char *name) {
    assert(t == ESP_PARTITION_TYPE_DATA && s == ESP_PARTITION_SUBTYPE_ANY);
    return !strcmp(name, "model") ? &user : NULL;
}
int esp_partition_read(const esp_partition_t *p, size_t off, void *buf, size_t n) {
    assert(display_paused);
    assert(p == &user && off + n <= sizeof flash);
    if (fail_read) return -1;
    memcpy(buf, flash + off, n); return 0;
}
int esp_partition_write(const esp_partition_t *p, size_t off, const void *buf, size_t n) {
    assert(display_paused);
    assert(p == &user && off + n <= sizeof flash);
    const uint8_t *b = buf;
    for (size_t i = 0; i < n; i++) { assert((flash[off + i] & b[i]) == b[i]); flash[off + i] &= b[i]; }
    return 0;
}
int esp_partition_erase_range(const esp_partition_t *p, size_t off, size_t n) {
    assert(display_paused);
    assert(p == &user && off % 4096 == 0 && n == 4096 && off + n <= sizeof flash);
    memset(flash + off, 255, n); erases++; return 0;
}
int esp_cache_get_alignment(unsigned caps, size_t *n) { (void)caps; *n = 128; return 0; }
bool esp_ptr_internal(const void *p) { return p != NULL; }
bool sdmmc_host_check_buffer_alignment(int s, const void *p, size_t n) { (void)s; (void)n; return p != NULL; }
int sdmmc_host_do_transaction(int s, sdmmc_command_t *cmd) { (void)s; (void)cmd; transactions++; return 0; }
int sdmmc_get_status(sdmmc_card_t *p) { assert(p == &card); return inserted ? 0 : -1; }
void gpio_hold_dis(int pin) { assert(pin >= 20 && pin <= 25); }
int esp_vfs_fat_sdmmc_mount(const char *p, const sdmmc_host_t *h, const sdmmc_slot_config_t *s,
                          const esp_vfs_fat_mount_config_t *cfg, sdmmc_card_t **out) {
    assert(!strcmp(p, "/sd") && h->slot == 0 && s->width == 4 && s->gpio_cd == -1);
    assert(s->clk == 24 && s->cmd == 25 && s->d0 == 20 && s->d1 == 21 && s->d2 == 22 && s->d3 == 23);
    assert(h->dma_aligned_buffer && h->unaligned_multi_block_rw_max_chunk_size == 8);
    assert(cfg->disk_status_check_enable && !cfg->format_if_mount_failed);
    if (!inserted) return -1;
    *out = &card; return 0;
}
int esp_vfs_fat_sdcard_unmount(const char *p, sdmmc_card_t *c) { assert(!strcmp(p, "/sd") && c == &card && !g_handles); return 0; }
int esp_vfs_fat_info(const char *p, uint64_t *total, uint64_t *left) { assert(!strcmp(p, "/sd")); *total = 1048576; *left = 524288; return 0; }
int esp_event_loop_create_default(void) { return 0; }
int esp_event_handler_register(esp_event_base_t b, int32_t id, test_event_fn fn, void *p) { (void)b; (void)id; (void)p; handler = fn; return 0; }
int esp_event_post(esp_event_base_t b, int32_t id, void *p, size_t n, unsigned wait) {
    (void)b; (void)id; (void)p; (void)n; (void)wait; pending = true; return 0;
}
int esp_timer_create(const esp_timer_create_args_t *c, esp_timer_handle_t *t) { assert(c->callback == card_tick); *t = &card; return 0; }
int esp_timer_start_periodic(esp_timer_handle_t t, uint64_t us) { (void)t; assert(us == 1000000); return 0; }
void esp_fill_random(void *p, size_t n) { memset(p, 0x5a, n); }
static void notify(void *p) { assert(p == &card && !lock_depth); callbacks++; }
static void poll_card(void) { card_tick(NULL); assert(pending); pending = false; handler(NULL, INK_SD_EVENT, 0, NULL); }
static void user_roundtrip(void) {
    hal_fs_volume_info_t vi;
    assert(g_cfg.block_count * g_cfg.block_size == sizeof flash);
    assert(!hal_fs_volume_info("/user", &vi) && vi.total_bytes == sizeof flash);
    printf("/user fresh total=%llu free=%llu\n", (unsigned long long)vi.total_bytes, (unsigned long long)vi.free_bytes);
    assert(vi.free_bytes == sizeof flash - 8192);
    assert(!hal_fs_mkdir("/user/member"));
    assert(hal_fs_exists("/user/missing") == 0);
    hal_file_t *f = hal_fs_open("/user/member/temp", HAL_O_READ | HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    assert(f && hal_fs_write(f, "hello", 5) == 5 && !hal_fs_sync(f));
    assert(hal_fs_tell(f) == 5 && !hal_fs_seek(f, 0, SEEK_SET));
    char b[16] = {0}; assert(hal_fs_read(f, b, sizeof b) == 5 && !strcmp(b, "hello"));
    assert(!hal_fs_close(f) && !hal_fs_rename("/user/member/temp", "/user/member/saved"));
    assert(hal_fs_size("/user/member/saved") == 5 && hal_fs_exists("/user/member/saved") == 1);
    hal_dir_t *d = hal_fs_opendir("/user/member"); hal_dirent_t e;
    assert(d && hal_fs_readdir(d, &e) == 1 && !strcmp(e.name, "saved") && e.size == 5);
    assert(hal_fs_readdir(d, &e) == 0); hal_fs_closedir(d);
    assert(!lfs_unmount(&g_lfs) && !lfs_mount(&g_lfs, &g_cfg));
    assert(hal_fs_size("/user/member/saved") == 5 && !hal_fs_remove("/user/member/saved"));
    unsigned before = erases; assert(!lfs_unmount(&g_lfs));
    fail_read = true;
    assert(lfs_mount(&g_lfs, &g_cfg) == LFS_ERR_IO && before == erases);
    fail_read = false; assert(!lfs_mount(&g_lfs, &g_cfg));
}
static void card_roundtrip(void) {
    assert(!hal_fs_card_present());
    inserted = true; poll_card(); assert(hal_fs_card_present() && callbacks == 1);
    hal_fs_volume_info_t vi; assert(!hal_fs_volume_info("/content", &vi) && vi.free_bytes == 524288);
    assert(!hal_fs_volume_info("/cache", &vi));
    assert(!hal_fs_open("/contentious/escape", HAL_O_READ) && !hal_fs_open("/content/../escape", HAL_O_READ));
    sdmmc_command_t cmd = {.opcode = MMC_WRITE_BLOCK_SINGLE};
    unsigned before = transactions;
    assert(transaction(0, &cmd) == (CONFIG_INK_SD_READONLY ? ESP_ERR_NOT_SUPPORTED : ESP_OK));
    assert(transactions == before + (CONFIG_INK_SD_READONLY ? 0 : 1));
    if (CONFIG_INK_SD_READONLY) {
        assert(hal_fs_mkdir("/content/new") < 0 && !hal_fs_open("/content/new", HAL_O_CREATE | HAL_O_WRITE));
        assert(hal_fs_remove("/content/seed") < 0 && hal_fs_rename("/content/seed", "/content/moved") < 0);
    } else {
        assert(!hal_fs_mkdir("/content/new"));
        hal_file_t *f = hal_fs_open("/cache/output", HAL_O_CREATE | HAL_O_READ | HAL_O_WRITE | HAL_O_TRUNC);
        assert(f && hal_fs_write(f, "card", 4) == 4 && !hal_fs_sync(f) && !hal_fs_close(f));
        assert(!hal_fs_rename("/cache/output", "/cache/renamed") && !hal_fs_remove("/cache/renamed"));
    }
    hal_file_t *f = hal_fs_open("/content/seed", HAL_O_READ);
    assert(f && hal_fs_mtime("/content/seed") >= 0);
    inserted = false; poll_card(); assert(!hal_fs_card_present() && callbacks == 2 && g_card);
    char b[8]; assert(hal_fs_read(f, b, sizeof b) < 0 && transaction(0, &cmd) == ESP_ERR_NOT_SUPPORTED);
    assert(!hal_fs_close(f)); poll_card(); assert(!g_card);
    inserted = true; poll_card(); assert(hal_fs_card_present() && callbacks == 3);
    hal_dir_t *d = hal_fs_opendir("/content"); hal_dirent_t e; bool seed = false;
    assert(d);
    while (hal_fs_readdir(d, &e) > 0) if (!strcmp(e.name, "seed")) seed = e.size == 4;
    hal_fs_closedir(d); assert(seed);
}
int main(void) {
    strcpy(g_root, "storage-card-XXXXXX"); assert(mkdtemp(g_root));
    char p[1024]; snprintf(p, sizeof p, "%s/content", g_root); assert(!mkdir(p, 0755));
    snprintf(p, sizeof p, "%s/cache", g_root); assert(!mkdir(p, 0755));
    snprintf(p, sizeof p, "%s/content/seed", g_root);
    FILE *seed = fopen(p, "wb"); assert(seed && fwrite("seed", 1, 4, seed) == 4 && !fclose(seed));
    memset(flash, 255, sizeof flash); ink_s31_storage_init(); assert(g_mounted && erases);
    hal_fs_set_card_callback(notify, &card); user_roundtrip(); card_roundtrip();
    assert(!lfs_unmount(&g_lfs)); heap_caps_free(g_dma);
    assert(!remove(p));
    snprintf(p, sizeof p, "%s/content/new", g_root); if (!CONFIG_INK_SD_READONLY) assert(!rmdir(p));
    snprintf(p, sizeof p, "%s/content", g_root); assert(!rmdir(p));
    snprintf(p, sizeof p, "%s/cache", g_root); assert(!rmdir(p)); assert(!rmdir(g_root));
    puts("storage HAL OK"); return 0;
}
