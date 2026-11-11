/* Screenshot every registered app and settings presenter through real capabilities. */
#include <stdlib.h>
#include "ink/ui_shell.h"
#include "ink/sys/appreg.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include "ui_borders.h"

static bool captured;
static int requested;
static char frame[96];
static const char *const SETTINGS[] = {
    "shell.settings", "shell.display", "shell.sound", "shell.network", "shell.known_networks",
    "shell.bluetooth", "shell.voice", "shell.storage", "shell.about", "shell.privacy", "shell.device",
    "shell.licenses", "shell.notify_settings", "shell.time", "shell.time_edit", "shell.time_zones", "shell.return",
    "shell.family", "shell.kid_settings", "family.kid_apps", "family.kid_periods", "shell.wallpaper",
    "shell.records", "shell.runtime_records", "shell.ai_memory"
};
static void inspect(void) {
    lv_obj_update_layout(lv_screen_active()); lv_obj_update_layout(lv_layer_sys());
    check_border_surfaces(lv_screen_active()); check_border_surfaces(lv_layer_sys());
}
static void capture(void *arg) { sim_display_snapshot(arg); captured = true; }
static void screenshot(const char *kind, const char *name) {
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "page settles before screenshot");
    on_ui(inspect);
    snprintf(frame, sizeof frame, "%s-%s", kind, name);
    captured = false; vtk_event_at(vtk_now() + SIM_MS(1), capture, frame);
    CHECK(WAIT_FOR(captured, 1000), "capture %s", frame);
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home();
    int index = 0;
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *app = ink_app_at(i);
        if (!ink_app_visible(app)) continue;
        if (index++ != requested) continue;
        char args[80]; snprintf(args, sizeof args, "{\"id\":\"%s\"}", app->id);
        CHECK(BY_HAND("app.open", args) && g_rc.status == INK_RC_DONE, "open registered app %s", app->id);
        screenshot("app", app->id);
        CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "return home after app");
    }
    for (size_t i = 0; i < sizeof SETTINGS / sizeof *SETTINGS; i++) {
        if (index++ != requested) continue;
        if (!strncmp(SETTINGS[i], "family.", 7)) CHECK(BY_HAND("shell.kid_settings", NULL) && g_rc.status == INK_RC_DONE, "open parent of child settings");
        if (!strcmp(SETTINGS[i], "shell.time_edit")) CHECK(BY_HAND("settings.set", "{\"id\":\"time.auto\",\"value\":0}") && g_rc.status == INK_RC_DONE, "enable manual date entry");
        CHECK(BY_HAND(SETTINGS[i], NULL) && g_rc.status == INK_RC_DONE, "open settings presenter %s", SETTINGS[i]);
        screenshot("settings", SETTINGS[i]);
        CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "return home after settings");
    }
    ink_probe_value("test.border.cases", index);
    CHECK(captured, "the requested case exists and was captured");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    const char *index = getenv("INK_BORDER_CASE"); requested = index ? atoi(index) : 0;
    product_main();
    osal_task_cfg_t cfg = {.name = "ui-borders", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start border screenshot sweep");
}
