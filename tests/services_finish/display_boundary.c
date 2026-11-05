#include "ink/hal_display.h"
bool ink_test_panel_fail;
static uint32_t panel_update(const hal_rect_t *area,const uint8_t *px,int stride,hal_waveform_t wf,uint32_t flags,hal_display_done_fn done,void *ctx) {
    if (ink_test_panel_fail) return 0;
    return hal_display_update(area,px,stride,wf,flags,done,ctx);
}
#define hal_display_update panel_update
#include "../../system/services/display/display_service.c"
