/* Home is a shell surface; its data and actions use the application framework. */
#include "ink/sys/i18n.h"
#include "ink/sys/appreg.h"
#include "ink/sys/ui.h"
#include "ink/sys/time.h"
#include <stdio.h>

static int card(int index, ink_card_t *out) {
    int cursor = 0;
    ink_alarm_t alarm;
    if (index || ink_alarm_next(&cursor, INK_ALARM, &alarm) != 1) return 0;
    snprintf(out->id, sizeof out->id, "upcoming");
    snprintf(out->title, sizeof out->title, TR("接下来"));
    snprintf(out->text, sizeof out->text, "%s %d:%02d", alarm.title, alarm.hour, alarm.min);
    out->rev = alarm.id;
    return 1;
}

static void create(lv_obj_t *screen, void *arg) { (void)arg; ink_shell_home_create(screen); }

const ink_activity_t ink_app_home = {
    .name = "home", .app = "home", .create = create, .destroy = ink_shell_home_destroy,
    .on_key = ink_shell_home_key, .on_swipe = ink_shell_home_swipe, .snapshot = ink_shell_home_snapshot,
    .settle = ink_shell_home_settle
};
const ink_app_def_t ink_appdef_home = {
    .id = "home", .title = TR_NOOP("主页"), .entry = &ink_app_home, .system_app = true, .child_ok = true, .keep = true, .card = card
};
