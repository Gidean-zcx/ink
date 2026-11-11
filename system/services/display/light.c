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

static osal_mutex_t *g_lock;
static osal_timer_t *g_timer;
static int g_level, g_target, g_warmth;
static bool g_off;

static void output(void) {
    int total = g_level * 10, warm = total * g_warmth / 100;
    hal_light_set(total - warm, warm);
}

/* Vendor periph/frontlight.cc:24,233-245: one brightness percentage point every 5 ms. */
static void step(void *arg) {
    (void)arg;
    if (osal_mutex_lock(g_lock, 0) != OSAL_OK) return;
    if (g_level != g_target) g_level += g_target > g_level ? 1 : -1;
    output();
    if (g_level == g_target) osal_timer_stop(g_timer);
    osal_mutex_unlock(g_lock);
}

static void apply(bool immediate) {
    int64_t level = ink_setting_get("display.light"), warmth = ink_setting_get("display.warmth");
    osal_mutex_lock(g_lock, OSAL_WAIT_FOREVER);
    g_target = g_off ? 0 : (int)level;
    g_warmth = (int)warmth;
    if (immediate) g_level = g_target; /* Vendor :154-164: restore without transition. */
    output();
    if (g_level != g_target) osal_timer_start(g_timer, 5000, 5000);
    else osal_timer_stop(g_timer);
    osal_mutex_unlock(g_lock);
}

void ink_light_off(void) { /* Vendor :215-230: stop transition before forcing both channels low. */
    if (!g_lock) { hal_light_set(0, 0); return; }
    osal_mutex_lock(g_lock, OSAL_WAIT_FOREVER);
    g_off = true;
    osal_timer_stop(g_timer);
    g_level = g_target = 0;
    output();
    osal_mutex_unlock(g_lock);
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
    osal_mutex_lock(g_lock, OSAL_WAIT_FOREVER);
    bool restore = g_off;
    g_off = false;
    osal_mutex_unlock(g_lock);
    if (restore) apply(true);
}

static const ink_setting_def_t LIGHT_SETTINGS[] = {
    {.id = "display.light", .title = TR_NOOP("亮度"), .type = INK_SET_INT, .min = 0, .max = 100, .def = 0, .child_may_change = true, .apply = on_setting,
     .apply_exec = INK_EXEC_DIRECT},
    {.id = "display.warmth", .title = TR_NOOP("暖光（越大越暖）"), .type = INK_SET_INT, .min = 0, .max = 100, .def = 50, .child_may_change = true,
     .apply = on_setting, .apply_exec = INK_EXEC_DIRECT},
};

int ink_light_init(void) {
    g_lock = osal_mutex_create();
    g_timer = osal_timer_create("frontlight", step, NULL);
    if (!g_lock || !g_timer) return -1;
    if (ink_settings_define(LIGHT_SETTINGS, 2) != 0) return -1;
    apply(true); /* the saved level */
    return ink_state_watch("power.state", on_power, NULL, INK_EXEC_DIRECT);
}
