/* MIT: Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd.
 * Copyright (c) 2025 Project Contributors. See docs/tasks/usb-disk-1/SOURCE.md.
 * SDMMC handoff and USB queue draining follow Metalio @874538e. */
#include "ink/hal_usb.h"
#include "driver/usb_serial_jtag.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "device/usbd_pvt.h"
#include "esp_vfs_fat.h"
#include "esp_event.h"
#include "driver/sdmmc_host.h"
#include <stdlib.h>

static tinyusb_msc_storage_handle_t ink_usb_storage;
static sdmmc_card_t *ink_usb_card;
static bool ink_usb_lent, ink_usb_up, ink_usb_done, ink_usb_attached, ink_usb_parked;
static bool ink_usb_was_connected, ink_usb_was_done;
static void (*ink_usb_callback)(void *);
static void *ink_usb_context;
static esp_err_t ink_usb_local_mount(const char *, const sdmmc_host_t *, const sdmmc_slot_config_t *,
                                    const esp_vfs_fat_mount_config_t *, sdmmc_card_t **);
static esp_err_t ink_usb_register(esp_event_base_t, int32_t, esp_event_handler_t, void *);
/* Share the existing card lock/handles without changing the storage HAL. */
#define esp_vfs_fat_sdmmc_mount ink_usb_local_mount
#define esp_event_handler_register ink_usb_register
#include "hal_storage_s31.c"
#undef esp_vfs_fat_sdmmc_mount
#undef esp_event_handler_register

static esp_err_t ink_usb_local_mount(const char *path, const sdmmc_host_t *host, const sdmmc_slot_config_t *slot,
                                    const esp_vfs_fat_mount_config_t *cfg, sdmmc_card_t **card) {
    return ink_usb_lent ? ESP_ERR_INVALID_STATE : esp_vfs_fat_sdmmc_mount(path, host, slot, cfg, card);
}
static void ink_usb_poll(void *arg, esp_event_base_t base, int32_t id, void *data) {
    card_event(arg, base, id, data);
    bool connected = hal_usb_host_connected(), done = hal_usb_host_done();
    LOCK();
    bool changed = connected != ink_usb_was_connected || done != ink_usb_was_done;
    ink_usb_was_connected = connected;
    ink_usb_was_done = done;
    void (*cb)(void *) = ink_usb_callback;
    void *ctx = ink_usb_context;
    osal_mutex_unlock(g_lock);
    if (changed && cb) cb(ctx);
}
static esp_err_t ink_usb_register(esp_event_base_t base, int32_t id, esp_event_handler_t fn, void *arg) {
    (void)fn;
    return esp_event_handler_register(base, id, ink_usb_poll, arg);
}
bool hal_usb_host_connected(void) {
    return usb_serial_jtag_is_connected() || __atomic_load_n(&ink_usb_attached, __ATOMIC_ACQUIRE);
}
bool hal_usb_host_done(void) {
    return __atomic_load_n(&ink_usb_done, __ATOMIC_ACQUIRE) || !hal_usb_host_connected();
}
void hal_usb_set_callback(void (*cb)(void *), void *ctx) {
    LOCK();
    ink_usb_callback = cb;
    ink_usb_context = ctx;
    osal_mutex_unlock(g_lock);
}
static void ink_usb_event(tinyusb_event_t *event, void *arg) {
    (void)arg;
    if (event->id == TINYUSB_EVENT_ATTACHED) __atomic_store_n(&ink_usb_attached, true, __ATOMIC_RELEASE);
    if (event->id == TINYUSB_EVENT_DETACHED) {
        __atomic_store_n(&ink_usb_attached, false, __ATOMIC_RELEASE);
        __atomic_store_n(&ink_usb_done, true, __ATOMIC_RELEASE);
    }
}
/* Keep eject on the existing service path; never auto-mount inside TinyUSB. */
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    (void)lun; (void)power_condition;
    if (load_eject && !start) __atomic_store_n(&ink_usb_done, true, __ATOMIC_RELEASE);
    return true;
}
/* Adapted complete InitRawCardForMsc: retain the mounted card's host/DMA setup. */
static sdmmc_card_t *ink_usb_raw_card(sdmmc_host_t host) {
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.clk = 24; slot.cmd = 25; slot.d0 = 20; slot.d1 = 21; slot.d2 = 22; slot.d3 = 23;
    slot.width = 4; slot.flags = 0;
    const int pins[] = {24, 25, 20, 21, 22, 23};
    for (unsigned i = 0; i < sizeof pins / sizeof pins[0]; i++) gpio_hold_dis(pins[i]);
    host.do_transaction = sdmmc_host_do_transaction;
    sdmmc_card_t *card = calloc(1, sizeof(sdmmc_card_t));
    if (card == NULL) return NULL;
    esp_err_t ret = (*host.init)();
    if (ret == ESP_ERR_INVALID_STATE) {
        sdmmc_host_deinit();
        ret = (*host.init)();
    }
    if (ret != ESP_OK) {
        free(card);
        return NULL;
    }
    ret = sdmmc_host_init_slot(host.slot, &slot);
    if (ret != ESP_OK) {
        sdmmc_host_deinit();
        free(card);
        return NULL;
    }
    ret = sdmmc_card_init(&host, card);
    if (ret != ESP_OK) {
        sdmmc_host_deinit();
        free(card);
        return NULL;
    }
    ret = sdmmc_get_status(card);
    if (ret != ESP_OK) {
        sdmmc_host_deinit();
        free(card);
        return NULL;
    }
    return card;
}
/* Vendor ParkUsbTask: drain accepted events and deferred writes before teardown. */
static void ink_usb_park(void *arg) {
    (void)arg;
    if (tud_task_event_ready()) {
        usbd_defer_func(ink_usb_park, NULL, false);
        return;
    }
    __atomic_store_n(&ink_usb_parked, true, __ATOMIC_RELEASE);
    vTaskSuspend(NULL);
}
static int ink_usb_reclaim(void) {
    if (ink_usb_up) {
        __atomic_store_n(&ink_usb_parked, false, __ATOMIC_RELEASE);
        tud_disconnect();
        usbd_defer_func(ink_usb_park, NULL, false);
        TickType_t start = xTaskGetTickCount();
        while (!__atomic_load_n(&ink_usb_parked, __ATOMIC_ACQUIRE)) {
            if (xTaskGetTickCount() - start >= pdMS_TO_TICKS(2000)) return -1;
            vTaskDelay(1);
        }
        if (tinyusb_driver_uninstall() != ESP_OK) return -1;
        ink_usb_up = false;
        __atomic_store_n(&ink_usb_attached, false, __ATOMIC_RELEASE);
    }
    if (ink_usb_storage) {
        if (tinyusb_msc_delete_storage(ink_usb_storage) != ESP_OK) return -1;
        ink_usb_storage = NULL;
    }
    if (tinyusb_msc_uninstall_driver() != ESP_OK) return -1;
    if (ink_usb_card) {
        sdmmc_host_deinit();
        free(ink_usb_card);
        ink_usb_card = NULL;
    }
    ink_usb_lent = false;
    return 0;
}
int hal_usb_disk_start(void) {
    LOCK();
    if (ink_usb_lent || CONFIG_INK_SD_READONLY || !g_present || !g_card || g_handles || !hal_usb_host_connected()) DONE(-1);
    sdmmc_host_t host = g_card->host;
    if (esp_vfs_fat_sdcard_unmount("/sd", g_card) != ESP_OK) DONE(-1);
    g_card = NULL;
    __atomic_store_n(&g_present, false, __ATOMIC_RELEASE);
    ink_usb_lent = true;
    __atomic_store_n(&ink_usb_done, false, __ATOMIC_RELEASE);
    ink_usb_card = ink_usb_raw_card(host);
    tinyusb_msc_driver_config_t msc = {.user_flags.auto_mount_off = 1};
    if (!ink_usb_card || tinyusb_msc_install_driver(&msc) != ESP_OK) goto failed;
    tinyusb_msc_storage_config_t storage = {.mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
        .fat_fs = {.base_path = "/sd", .config = {.max_files = 12}, .do_not_format = true}, .medium.card = ink_usb_card};
    if (tinyusb_msc_new_storage_sdmmc(&storage, &ink_usb_storage) != ESP_OK) goto failed;
    /* Factory S31: dedicated HS UTMI PHY, USB task on CPU0, 4096-byte internal stack. */
    tinyusb_config_t cfg = TINYUSB_CONFIG_HIGH_SPEED(ink_usb_event, NULL);
    cfg.task = TINYUSB_TASK_CUSTOM(TINYUSB_DEFAULT_TASK_SIZE, TINYUSB_DEFAULT_TASK_PRIO, 0);
    if (tinyusb_driver_install(&cfg) != ESP_OK) goto failed;
    ink_usb_up = true;
    DONE(0);
failed:
    /* No USB task exists here. Restore the ordinary IDF mount, without formatting. */
    if (ink_usb_storage && tinyusb_msc_delete_storage(ink_usb_storage) != ESP_OK) DONE(-1);
    ink_usb_storage = NULL;
    tinyusb_msc_uninstall_driver();
    if (ink_usb_card) { sdmmc_host_deinit(); free(ink_usb_card); ink_usb_card = NULL; }
    ink_usb_lent = false;
    osal_mutex_unlock(g_lock);
    card_event(NULL, INK_SD_EVENT, 0, NULL);
    return -1;
}
int hal_usb_disk_stop(void) {
    LOCK();
    int r = ink_usb_lent ? ink_usb_reclaim() : 0;
    osal_mutex_unlock(g_lock);
    if (r) return -1;
    card_event(NULL, INK_SD_EVENT, 0, NULL);
    return hal_fs_card_present() ? 0 : -1;
}
