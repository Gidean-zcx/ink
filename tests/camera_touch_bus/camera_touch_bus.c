/* HOME during the camera viewfinder: the cover keys are read over the I2C bus the GC2145 sensor shares. Every short
 * (80 ms) press must arrive, at any phase of the preview cycle and while the viewfinder sets the sensor up; the
 * viewfinder must also not get slower. Real product, touch driver and camera model; only the presses are synthetic. */
#include <stdlib.h>
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

#define PHASES 24     /* presses spread over one old preview cycle (1.6 s) and about two kept-session cycles */
#define PHASE_MS 70
#define SETUP_PRESSES 6 /* presses during the first capture, which loads the sensor table (~0.4 s) */
#define SETUP_MS 80

static char page[32];
static void read_page(void) { sim_copy_name(page, sizeof page, ink_ui_page_name()); }
static bool on_page(const char *name) { on_ui(read_page); return !strcmp(page, name); }
static void home_down(void *arg) { (void)arg; sim_input_key(HAL_KEY_HOME, true); }
static void home_up(void *arg) { (void)arg; sim_input_key(HAL_KEY_HOME, false); }
static void press_home(void) { /* the script's "key home": 80 ms */
    uint64_t t = vtk_now() + SIM_MS(1);
    vtk_event_at(t, home_down, NULL);
    vtk_event_at(t + SIM_MS(80), home_up, NULL);
}
static bool open_camera(void) {
    CHECK(BY_HAND("app.open", "{\"id\":\"camera\"}") && g_rc.status == INK_RC_DONE, "camera opens: %s", ink_rc_name(g_rc.status));
    return WAIT_FOR(on_page("Camera"), 3000);
}
static void wait_new_frame(void) { /* the phase reference: a preview frame has just arrived */
    unsigned seen = sim_camera_captures();
    for (int i = 0; i < 5000 && sim_camera_captures() == seen; i++) delay_ms(1);
}
static void home_arrives(const char *when, int ms) {
    press_home();
    CHECK(WAIT_FOR(on_page("home"), 3000), "HOME pressed %d ms %s reaches home (page %s)", ms, when, page);
    CHECK(WAIT_FOR(!sim_camera_busy(), 3000), "leaving the camera ends its session");
}

static void driver(void *unused) {
    (void)unused;
    test_start_home();
    for (int i = 0; i < PHASES; i++) {
        if (!open_camera()) continue;
        unsigned first = sim_camera_captures();
        CHECK(WAIT_FOR(sim_camera_captures() >= first + 2, 6000), "viewfinder runs before press %d", i);
        wait_new_frame();
        delay_ms(i * PHASE_MS);
        home_arrives("after a preview frame", i * PHASE_MS);
    }
    for (int i = 0; i < SETUP_PRESSES; i++) {
        unsigned first = sim_camera_captures();
        if (!open_camera()) continue;
        delay_ms(i * SETUP_MS);
        CHECK(sim_camera_captures() == first, "press %d falls in the first capture's set-up", i);
        home_arrives("after the viewfinder opens", i * SETUP_MS);
    }
    if (open_camera()) {
        delay_ms(2000);
        unsigned first = sim_camera_captures();
        delay_ms(20000);
        unsigned frames = sim_camera_captures() - first;
        sim_note("camera-touch-bus: %u preview frames in 20 s", frames);
        ink_probe_value("camera_touch_bus.frames_20s", frames);
        CHECK(frames >= 20, "viewfinder keeps at least 20 frames in 20 s: %u (13 when every frame reloaded the table)", frames);
    }
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    const osal_task_cfg_t cfg = {.name = "camera-touch-bus", .fn = driver, .stack_bytes = 8192,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the HOME check");
}
