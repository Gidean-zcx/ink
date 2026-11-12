#ifndef INK_USB_TEST_IDF_H
#define INK_USB_TEST_IDF_H
#include "storage_idf.h"
typedef unsigned TickType_t;
typedef void *tinyusb_msc_storage_handle_t;
enum { TINYUSB_EVENT_ATTACHED, TINYUSB_EVENT_DETACHED, TINYUSB_MSC_STORAGE_MOUNT_USB };
typedef struct { int id; } tinyusb_event_t;
typedef struct { struct { unsigned auto_mount_off; } user_flags; } tinyusb_msc_driver_config_t;
typedef struct {
    int mount_point;
    struct { const char *base_path; esp_vfs_fat_mount_config_t config; bool do_not_format; } fat_fs;
    struct { sdmmc_card_t *card; } medium;
} tinyusb_msc_storage_config_t;
typedef struct { unsigned size, priority, xCoreID; } test_usb_task_t;
typedef struct { test_usb_task_t task; void (*event_cb)(tinyusb_event_t *, void *); int port; } tinyusb_config_t;
#define TINYUSB_DEFAULT_TASK_SIZE 4096
#define TINYUSB_DEFAULT_TASK_PRIO 5
#define TINYUSB_CONFIG_HIGH_SPEED(fn, arg) (tinyusb_config_t){.event_cb = fn, .port = 1}
#define TINYUSB_TASK_CUSTOM(s, p, c) (test_usb_task_t){s, p, c}
#define pdMS_TO_TICKS(ms) (ms)
bool usb_serial_jtag_is_connected(void);
int tinyusb_msc_install_driver(const tinyusb_msc_driver_config_t *);
int tinyusb_msc_uninstall_driver(void);
int tinyusb_msc_new_storage_sdmmc(const tinyusb_msc_storage_config_t *, tinyusb_msc_storage_handle_t *);
int tinyusb_msc_delete_storage(tinyusb_msc_storage_handle_t);
int tinyusb_driver_install(const tinyusb_config_t *);
int tinyusb_driver_uninstall(void);
bool tud_task_event_ready(void);
bool tud_disconnect(void);
void usbd_defer_func(void (*)(void *), void *, bool);
TickType_t xTaskGetTickCount(void);
void vTaskSuspend(void *);
void vTaskDelay(unsigned);
#endif
