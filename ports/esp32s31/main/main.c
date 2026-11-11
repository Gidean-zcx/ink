#include <stdio.h>
#ifdef ESP_PLATFORM /* the host test of app_main (tests/boot.c) has no interrupt table */
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#include "ink/osal.h"
#include "ink/hal_device.h"
#include "ink/hal_display.h"
#include "s31_safety.h"
void ink_main(void);
void ink_s31_osal_selftest(void);
void ink_s31_board_init(void);
void ink_s31_assets_init(void);
void ink_s31_storage_init(void);
void app_main(void) {
    ink_s31_boot_begin();
    ink_s31_assets_init();
    ink_s31_storage_init();
    ink_s31_osal_selftest();
    ink_s31_board_init();
    hal_panel_info_t panel;
    hal_display_get_info(&panel);
    if (panel.width <= 0 || panel.height <= 0) {
        osal_panic("boot: panel unavailable");
    }
    ink_main();
#ifdef ESP_PLATFORM
    /* Which interrupts keep running while the flash is busy (IRAM) and on which core, once start-up is over: a cache
     * access error came from core 1 during a flash read at boot (trial install 12, 2026-11-11). Console only. */
    vTaskDelay(pdMS_TO_TICKS(15000));
    esp_intr_dump(stdout);
#endif
}
