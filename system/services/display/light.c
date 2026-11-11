/*
 * Front light settings: display.light (how bright) and display.warmth (how
 * warm), put into effect with the panel's two LED channels. The platform
 * turns the light off for light sleep; it comes back with the active state.
 */
#include <string.h>

#include "ink/sys/i18n.h"
#include "ink/hal_power.h"
#include "ink/osal.h"
#include "ink/sys/display.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"

static INK_PSRAM_BSS ink_svc_deadline_t g_timer;
static int g_level, g_target, g_warmth;
static bool g_off;

static void output(void) {
    int total = g_level * 10, warm = total * g_warmth / 100;
    hal_light_set(total - warm, warm);
}

/* Vendor periph/frontlight.cc:24,233-245: one brightness percentage point every 5 ms. */
static void step(void *arg) {
    (void)arg;
    ink_svc_lock();
    if (g_level != g_target) g_level += g_target > g_level ? 1 : -1;
    output();
    bool moving = g_level != g_target;
    ink_svc_unlock();
    if (moving) ink_svc_at(&g_timer, osal_now_us() + 5000, step, NULL);
}

static void apply(bool immediate) {
    int64_t level = ink_setting_get("display.light"), warmth = ink_setting_get("display.warmth");
    ink_svc_lock();
    g_target = g_off ? 0 : (int)level;
    g_warmth = (int)warmth;
    if (immediate) g_level = g_target; /* Vendor :154-164: restore without transition. */
    output();
    bool moving = g_level != g_target;
    ink_svc_unlock();
    if (moving) ink_svc_at(&g_timer, osal_now_us() + 5000, step, NULL);
    else ink_svc_cancel(&g_timer);
}

void ink_light_off(void) { /* Vendor :215-230: stop transition before forcing both channels low. */
    ink_svc_lock();
    g_off = true;
    g_level = g_target = 0;
    output();
    ink_svc_unlock();
    ink_svc_cancel(&g_timer);
}

static void on_setting(void *ctx, int64_t v) {
    (void)ctx;
    (void)v;
    apply(false);
}

static void on_power(void *ctx, int h, const char *id) { /* after a wake the light is on again */
    (void)ctx;
    (void)id;
    char s[16];
    if (ink_state_get_str(h, s, sizeof s) <= 0) return;
    if (!strcmp(s, "locked")) { ink_light_off(); return; }
    ink_svc_lock();
    bool restore = g_off;
    g_off = false;
    ink_svc_unlock();
    if (restore) apply(true);
}

static const ink_setting_def_t LIGHT_SETTINGS[] = {
    {.id = "display.light", .title = TR_NOOP("亮度"), .type = INK_SET_INT, .min = 0, .max = 100, .def = 0, .child_may_change = true, .apply = on_setting,
     .apply_exec = INK_EXEC_DIRECT},
    {.id = "display.warmth", .title = TR_NOOP("暖光（越大越暖）"), .type = INK_SET_INT, .min = 0, .max = 100, .def = 50, .child_may_change = true,
     .apply = on_setting, .apply_exec = INK_EXEC_DIRECT},
};

int ink_light_init(void) {
    if (ink_svc_init() != 0) return -1;
    if (ink_settings_define(LIGHT_SETTINGS, 2) != 0) return -1;
    apply(true); /* the saved level */
    return ink_state_watch("power.state", on_power, NULL, INK_EXEC_DIRECT);
}
