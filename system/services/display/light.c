/*
 * Front light settings: display.light (how bright) and display.warmth (how
 * warm), put into effect with the panel's two LED channels. The platform
 * turns the light off for light sleep; it comes back with the active state.
 */
#include <string.h>

#include "ink/sys/i18n.h"
#include "ink/hal_power.h"
#include "ink/sys/display.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"

static void apply(void) {
    int64_t level = ink_setting_get("display.light"), warmth = ink_setting_get("display.warmth");
    int total = (int)level * 10; /* 0..1000 permille */
    int warm = (int)(total * warmth / 100);
    hal_light_set(total - warm, warm);
}

static void on_setting(void *ctx, int64_t v) {
    (void)ctx;
    (void)v;
    apply();
}

static void on_power(void *ctx, int h, const char *id) { /* after a wake the light is on again */
    (void)ctx;
    (void)id;
    char s[16];
    if (ink_setting_get("display.light") && ink_state_get_str(h, s, sizeof s) > 0 && strcmp(s, "locked")) apply();
}

static const ink_setting_def_t LIGHT_SETTINGS[] = {
    {.id = "display.light", .title = TR_NOOP("亮度"), .type = INK_SET_INT, .min = 0, .max = 100, .def = 0, .child_may_change = true, .apply = on_setting,
     .apply_exec = INK_EXEC_DIRECT},
    {.id = "display.warmth", .title = TR_NOOP("暖光（越大越暖）"), .type = INK_SET_INT, .min = 0, .max = 100, .def = 50, .child_may_change = true,
     .apply = on_setting, .apply_exec = INK_EXEC_DIRECT},
};

int ink_light_init(void) {
    if (ink_settings_define(LIGHT_SETTINGS, 2) != 0) return -1;
    if (ink_setting_get("display.light")) apply(); /* the saved level */
    return ink_state_watch("power.state", on_power, NULL, INK_EXEC_DIRECT);
}
