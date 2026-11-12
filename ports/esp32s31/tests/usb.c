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
#include "usb_idf.h"
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
#include "../hal_usb_s31.c"
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
static esp_event_handler_t handler;
static bool pending;
static unsigned lock_depth;
static bool fail_read;
/* The vendor's flash drain: every flash operation runs with the I2C1 lock held (s31_flash_begin). */
static bool draining;
static unsigned drains;
bool s31_flash_begin(void) { assert(!draining); draining = true; drains++; return true; }
void s31_flash_end(bool taken) { assert(taken && draining); draining = false; }
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
    assert(p == &user && off + n <= sizeof flash && draining);
    if (fail_read) return -1;
    memcpy(buf, flash + off, n); return 0;
}
int esp_partition_write(const esp_partition_t *p, size_t off, const void *buf, size_t n) {
    assert(p == &user && off + n <= sizeof flash && draining);
    const uint8_t *b = buf;
    for (size_t i = 0; i < n; i++) { assert((flash[off + i] & b[i]) == b[i]); flash[off + i] &= b[i]; }
    return 0;
}
int esp_partition_erase_range(const esp_partition_t *p, size_t off, size_t n) {
    assert(p == &user && off % 4096 == 0 && n == 4096 && off + n <= sizeof flash && draining);
    memset(flash + off, 255, n); erases++; return 0;
}
int esp_cache_get_alignment(unsigned caps, size_t *n) { (void)caps; *n = 128; return 0; }
bool esp_ptr_internal(const void *p) { return p != NULL; }
bool sdmmc_host_check_buffer_alignment(int s, const void *p, size_t n) { (void)s; (void)n; return p != NULL; }
int sdmmc_host_do_transaction(int s, sdmmc_command_t *cmd) { (void)s; (void)cmd; transactions++; return 0; }
int sdmmc_get_status(sdmmc_card_t *p) { assert(p); return inserted ? 0 : -1; }
void gpio_hold_dis(int pin) { assert(pin >= 20 && pin <= 25); }
int esp_vfs_fat_sdmmc_mount(const char *p, const sdmmc_host_t *h, const sdmmc_slot_config_t *s,
                          const esp_vfs_fat_mount_config_t *cfg, sdmmc_card_t **out) {
    assert(!strcmp(p, "/sd") && h->slot == 0 && s->width == 4 && s->gpio_cd == -1);
    assert(s->clk == 24 && s->cmd == 25 && s->d0 == 20 && s->d1 == 21 && s->d2 == 22 && s->d3 == 23);
    assert(h->dma_aligned_buffer && h->unaligned_multi_block_rw_max_chunk_size == 8);
    assert(cfg->disk_status_check_enable && !cfg->format_if_mount_failed);
    if (!inserted) return -1;
    card.host = *h; *out = &card; return 0;
}
int esp_vfs_fat_sdcard_unmount(const char *p, sdmmc_card_t *c) { assert(!strcmp(p, "/sd") && c == &card && !g_handles); return 0; }
int esp_vfs_fat_info(const char *p, uint64_t *total, uint64_t *left) { assert(!strcmp(p, "/sd")); *total = 1048576; *left = 524288; return 0; }
int esp_event_loop_create_default(void) { return 0; }
int esp_event_handler_register(esp_event_base_t b, int32_t id, esp_event_handler_t fn, void *p) { (void)b; (void)id; (void)p; handler = fn; return 0; }
int esp_event_post(esp_event_base_t b, int32_t id, void *p, size_t n, unsigned wait) {
    (void)b; (void)id; (void)p; (void)n; (void)wait; pending = true; return 0;
}
int esp_timer_create(const esp_timer_create_args_t *c, esp_timer_handle_t *t) { assert(c->callback == card_tick); *t = &card; return 0; }
int esp_timer_start_periodic(esp_timer_handle_t t, uint64_t us) { (void)t; assert(us == 1000000); return 0; }
void esp_fill_random(void *p, size_t n) { memset(p, 0x5a, n); }
static void notify(void *p) { assert(p == &card && !lock_depth); callbacks++; }
static void poll_card(void) { card_tick(NULL); assert(pending); pending = false; handler(NULL, INK_SD_EVENT, 0, NULL); }
static bool computer, msc_installed, usb_running, stop_requested, fail_install;
static unsigned usb_callbacks, deferred_writes, completed_writes;
static void (*usb_event_cb)(tinyusb_event_t *, void *);
static void (*deferred)(void *);
static void *deferred_arg;
static unsigned ticks;
bool usb_serial_jtag_is_connected(void) { return computer; }
int sdmmc_host_init(void) { return 0; }
int sdmmc_host_deinit(void) { return 0; }
int sdmmc_host_init_slot(int slot, const sdmmc_slot_config_t *cfg) {
    assert(slot == 0 && cfg->clk == 24 && cfg->cmd == 25 && cfg->d0 == 20 && cfg->width == 4);
    return 0;
}
int sdmmc_card_init(const sdmmc_host_t *host, sdmmc_card_t *raw) {
    assert(host->dma_aligned_buffer == g_dma && host->do_transaction == sdmmc_host_do_transaction);
    raw->host = *host;
    return 0;
}
int tinyusb_msc_install_driver(const tinyusb_msc_driver_config_t *cfg) {
    assert(cfg->user_flags.auto_mount_off && !msc_installed);
    msc_installed = true;
    return 0;
}
int tinyusb_msc_uninstall_driver(void) { msc_installed = false; return 0; }
int tinyusb_msc_new_storage_sdmmc(const tinyusb_msc_storage_config_t *cfg, tinyusb_msc_storage_handle_t *out) {
    assert(msc_installed && cfg->mount_point == TINYUSB_MSC_STORAGE_MOUNT_USB);
    assert(cfg->fat_fs.do_not_format && !cfg->fat_fs.config.format_if_mount_failed);
    assert(cfg->medium.card == ink_usb_card && !g_card && !g_handles && !g_present);
    *out = cfg->medium.card;
    return 0;
}
int tinyusb_msc_delete_storage(tinyusb_msc_storage_handle_t storage) {
    assert(!usb_running && !deferred_writes && storage == ink_usb_card);
    return 0;
}
int tinyusb_driver_install(const tinyusb_config_t *cfg) {
    assert(cfg->port == 1 && cfg->task.size == 4096 && cfg->task.xCoreID == 0);
    if (fail_install) return -1;
    assert(ink_usb_storage && !usb_running);
    usb_event_cb = cfg->event_cb;
    usb_running = true;
    return 0;
}
int tinyusb_driver_uninstall(void) {
    assert(ink_usb_parked && !deferred_writes && usb_running);
    usb_running = false;
    return 0;
}
bool tud_task_event_ready(void) {
    if (!deferred_writes) return false;
    deferred_writes--;
    completed_writes++;
    return true;
}
bool tud_disconnect(void) { assert(usb_running); return true; }
void usbd_defer_func(void (*fn)(void *), void *arg, bool isr) { assert(!isr); deferred = fn; deferred_arg = arg; }
TickType_t xTaskGetTickCount(void) { return ticks; }
void vTaskSuspend(void *task) { assert(!task && ink_usb_parked); }
void vTaskDelay(unsigned delay) {
    ticks += delay;
    void (*fn)(void *) = deferred;
    deferred = NULL;
    if (fn) fn(deferred_arg);
}
static void usb_notify(void *arg) {
    assert(arg == &card && !lock_depth);
    usb_callbacks++;
    stop_requested = hal_usb_host_done();
}
static void check_local_write(void) {
    hal_file_t *f = hal_fs_open("/content/from-device.txt", HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    assert(f && hal_fs_write(f, "synthetic", 9) == 9 && !hal_fs_close(f));
}
static void check_lent(void) {
    assert(!hal_fs_open("/content/from-device.txt", HAL_O_WRITE | HAL_O_CREATE));
    assert(!hal_fs_open("/cache/new", HAL_O_WRITE | HAL_O_CREATE));
    assert(hal_fs_mkdir("/content/new") < 0);
    poll_card();
    assert(!g_card && ink_usb_storage && !g_present);
}
int main(void) {
    strcpy(g_root, "usb-card-XXXXXX"); assert(mkdtemp(g_root));
    char path[1024];
    snprintf(path, sizeof path, "%s/content", g_root); assert(!mkdir(path, 0755));
    snprintf(path, sizeof path, "%s/cache", g_root); assert(!mkdir(path, 0755));
    memset(flash, 255, sizeof flash);
    ink_s31_storage_init();
    hal_fs_set_card_callback(notify, &card);
    hal_usb_set_callback(usb_notify, &card);
    assert(hal_usb_disk_start() == -1);
    inserted = true; poll_card();
    assert(hal_fs_card_present() && callbacks == 1);
    assert(hal_usb_disk_start() == -1);
    unsigned before = usb_callbacks;
    computer = true; poll_card(); assert(usb_callbacks == before + 1 && !stop_requested);
    check_local_write();
    hal_file_t *held = hal_fs_open("/content/from-device.txt", HAL_O_READ);
    assert(held && hal_usb_disk_start() == -1 && !hal_fs_close(held));
    fail_install = true;
    assert(hal_usb_disk_start() == -1 && hal_fs_card_present());
    fail_install = false;
    check_local_write();
    assert(!hal_usb_disk_start()); check_lent();
    tinyusb_event_t attached = {.id = TINYUSB_EVENT_ATTACHED}; usb_event_cb(&attached, NULL);
    assert(!hal_usb_host_done());
    hal_file_t *user_file = hal_fs_open("/user/synthetic", HAL_O_WRITE | HAL_O_CREATE);
    assert(user_file && hal_fs_write(user_file, "safe", 4) == 4 && !hal_fs_close(user_file));
    assert(tud_msc_start_stop_cb(0, 0, false, true) && hal_usb_host_done());
    poll_card(); assert(stop_requested);
    deferred_writes = 2;
    assert(!hal_usb_disk_stop() && completed_writes == 2 && hal_fs_card_present());
    check_local_write();
    stop_requested = false;
    assert(!hal_usb_disk_start()); check_lent();
    usb_event_cb(&attached, NULL);
    computer = false;
    tinyusb_event_t detached = {.id = TINYUSB_EVENT_DETACHED}; usb_event_cb(&detached, NULL);
    poll_card(); assert(stop_requested);
    assert(!hal_usb_disk_stop() && hal_fs_card_present());
    check_local_write();
    assert(!lfs_unmount(&g_lfs)); heap_caps_free(g_dma);
    snprintf(path, sizeof path, "%s/content/from-device.txt", g_root); assert(!remove(path));
    snprintf(path, sizeof path, "%s/content", g_root); assert(!rmdir(path));
    snprintf(path, sizeof path, "%s/cache", g_root); assert(!rmdir(path));
    assert(!rmdir(g_root));
    puts("USB HAL: loan blocks local writes; eject and unplug restore; queued writes drain; failed start restores");
    return 0;
}
