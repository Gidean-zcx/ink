/* Binary previews must leave old gray pixels for the following GL16 pass. */
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/rotation.h"
#include "sim.h"

static osal_queue_t *results;
static hal_panel_info_t panel;
static uint8_t visible[1216 * 1216];

static void check(bool ok) {
    ink_probe_count(ok ? "test.pass" : "test.fail");
}

static void done(void *ctx, const hal_display_result_t *r) {
    (void)ctx;
    check(osal_queue_send(results, r, 0) == OSAL_OK);
}

static bool update(const uint8_t *pixels, hal_waveform_t waveform, uint32_t flags) {
    const hal_rect_t area = {0, 0, 16, 16};
    hal_display_result_t r;
    if (!hal_display_update(&area, pixels, 16, waveform, flags, done, NULL)) return false;
    return osal_queue_recv(results, &r, 5000000) == OSAL_OK && r.ok;
}

static int pixel(int x, int y) {
    int w, h, lx, ly;
    if (!sim_display_visible(visible, sizeof visible, &w, &h)) return -1;
    ink_native_to_logical(panel.width, panel.height, panel.rotation_deg, x, y, &lx, &ly);
    return visible[(size_t)ly * w + lx];
}

void ink_main(void) {
    hal_display_get_info(&panel);
    results = osal_queue_create(sizeof(hal_display_result_t), 4);
    if (!results) osal_panic("preview test queue");
    uint8_t pixels[16 * 16];
    memset(pixels, 255, sizeof pixels);
    for (int y = 0; y < 16; y++) memset(pixels + y * 16, 85, 8);
    check(update(pixels, HAL_WF_GC16, 0));
    int gray = pixel(2, 2);
    check(gray == 85);
    memset(pixels, 0, sizeof pixels);
    check(update(pixels, HAL_WF_DU, HAL_UPD_FLAG_BINARY_PREVIEW));
    check(pixel(2, 2) == gray);
    check(pixel(12, 2) < 64);
    check(update(pixels, HAL_WF_GL16, 0));
    check(pixel(2, 2) < 64);
    memset(pixels, 255, sizeof pixels);
    check(!update(pixels, HAL_WF_GL16, HAL_UPD_FLAG_BINARY_PREVIEW));
    check(pixel(2, 2) < 64);
    check(update(pixels, HAL_WF_GC16, 0));
    check(pixel(2, 2) == 255);
    ink_probe_count("test.finished");
}
