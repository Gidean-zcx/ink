/* Real UI/service, with either the existing sensor model or the S31 DVP recorder. */
#include "ink/hal_camera.h"
#include "ink/sys/ui.h"
#include "ink/probe.h"
#include "sim.h"
#ifdef INK_PREVIEW_DVP
void ink_test_camera_session(const int *active); /* the simulator's camera model has its own session too */
#define hal_camera_info ink_test_camera_info
#define hal_camera_capture ink_test_camera_capture
#define hal_camera_session ink_test_camera_session
#include "../hal_camera_s31.c"
#undef hal_camera_info
#undef hal_camera_capture
#undef hal_camera_session
#undef S
#include "camera_preview_dvp.inc"
#else
#define ink_test_camera_info hal_camera_info
#define ink_test_camera_capture hal_camera_capture
#define ink_test_camera_session hal_camera_session
#endif
static unsigned detections, captured_frames, fast_hints, gray_hints;
static bool measuring, overlap;
bool fixture_camera_info(hal_camera_info_t *out) {
    if (++detections == 1) { *out = (hal_camera_info_t){0}; return false; }
    return ink_test_camera_info(out);
}
void fixture_camera_session(const int *active);
void fixture_camera_session(const int *active) { ink_test_camera_session(active); }
int fixture_camera_capture(uint8_t *buf, size_t cap, uint32_t timeout) {
    if (measuring && ink_display_busy()) overlap = true;
    int n = ink_test_camera_capture(buf, cap, timeout);
    ink_probe_value("camera.fixture.capture.rc", n);
    if (measuring && n > 0) captured_frames++;
    return n;
}
static void fixture_hint(lv_obj_t *obj, ink_refresh_t intent);
#define ink_ui_hint_obj fixture_hint
#ifndef INK_CAMERA_APP_SOURCE
#define INK_CAMERA_APP_SOURCE "../../../system/apps/camera/camera.c"
#endif
#include INK_CAMERA_APP_SOURCE
#undef ink_ui_hint_obj
static void fixture_hint(lv_obj_t *obj, ink_refresh_t intent) {
    if (obj == view) {
        if (intent == INK_REFRESH_FAST) fast_hints++;
        if (intent == INK_REFRESH_UI) gray_hints++;
    }
    ink_ui_hint_obj(obj, intent);
}
#define ink_main product_main
#include "../../../system/boot/main.c"
#undef ink_main
#include "../../../tests/integration/whole_product.h"
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_cap_find("app.open") && ink_ui_page_current(), 15000), "boot ready");
    test_start_home();
    CHECK(!ink_cap_find("camera.capture") && detections == 1, "boot transient failure is cached initially");
    CHECK(BY_HAND("app.open", "{\"id\":\"camera\"}") && g_rc.status == INK_RC_DONE, "open camera after failed boot detection");
    CHECK(WAIT_FOR(ink_cap_find("camera.capture") && ink_cap_find("camera.scan") && view && lv_obj_get_child_count(view), 15000),
          "page entry reprobes and restores capture, scan and live controls");
    CHECK(detections == 2, "exactly one page-entry reprobe");
    captured_frames = 0; measuring = true;
    uint64_t start = osal_now_us();
    delay_ms(6000);
    measuring = false;
    double fps = captured_frames * 1000000.0 / (double)(osal_now_us() - start);
    ink_probe_value("camera.preview.fps", fps);
    ink_probe_value("camera.preview.frames", captured_frames);
    CHECK(captured_frames >= 5 && fast_hints >= 5, "display-paced preview advances without the old 3 s period");
    CHECK(!overlap, "next preview capture waits for the previous panel refresh");
#ifdef INK_PREVIEW_DVP
    CHECK(fps >= 2.0, "S31 retained session exceeds 2 fps with 15 fps synthetic DVP input");
    CHECK(dvp_allocations == 1, "all viewfinder frames share one DMA buffer");
#endif
    CHECK(AI("camera.capture", NULL) && g_rc.status == INK_RC_DONE, "AI raw capture remains callable");
    CHECK(BY_HAND("camera.photo", NULL) && g_rc.status == INK_RC_DONE, "full-quality photo remains callable");
    CHECK(WAIT_FOR(gray_hints > 0, 5000), "saved-photo preview retains gray refresh");
#ifdef INK_PREVIEW_DVP
    CHECK(dvp_allocations == 1, "photo shares the continuous session");
#endif
    CHECK(BY_HAND("camera.scan", NULL), "scan remains callable on the same media executor");
    CHECK(!strcmp(ink_ui_page_name(), "Scan"), "scan presents its normal result page");
#ifdef INK_PREVIEW_DVP
    CHECK(dvp_allocations == 1 && dvp_releases == 0, "scan shares the live camera session");
#endif
    CHECK(BY_HAND("system.home", NULL), "leave camera app");
#ifdef INK_PREVIEW_DVP
    CHECK(WAIT_FOR(dvp_releases == dvp_allocations && !ink_s31_camera_busy(), 5000), "leaving camera releases the retained DMA buffer");
#endif
    CHECK(BY_HAND("app.open", "{\"id\":\"camera\"}"), "reopen camera");
    CHECK(WAIT_FOR(view && lv_obj_get_child_count(view), 15000), "preview resumes on reopen");
    ink_power_lock();
    CHECK(WAIT_FOR(ink_power_state() == INK_PWR_LOCKED, 5000), "lock with camera open");
#ifdef INK_PREVIEW_DVP
    ink_probe_value("camera.fixture.allocations", dvp_allocations);
    ink_probe_value("camera.fixture.releases", dvp_releases);
    ink_probe_value("camera.fixture.busy", ink_s31_camera_busy());
    CHECK(WAIT_FOR(dvp_releases == dvp_allocations && !ink_s31_camera_busy(), 5000), "locking releases the retained DMA buffer");
    ink_probe_value("camera.fixture.final.allocations", dvp_allocations);
    ink_probe_value("camera.fixture.final.releases", dvp_releases);
#endif
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "camera-preview-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start camera preview regression");
}
