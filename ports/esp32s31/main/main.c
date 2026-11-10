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
}
