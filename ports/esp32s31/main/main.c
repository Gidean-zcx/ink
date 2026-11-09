#include "ink/osal.h"
#include "ink/hal_device.h"
#include "ink/hal_display.h"
void ink_main(void);
void ink_s31_osal_selftest(void);
void app_main(void) {
    ink_s31_osal_selftest();
    hal_panel_info_t panel;
    hal_display_get_info(&panel);
    if (panel.width <= 0 || panel.height <= 0) {
        hal_log_write("没有屏幕：端口骨架已启动，硬件驱动尚未接入");
        return;
    }
    ink_main();
}
