/* Home is a shell surface; its data and actions use the application framework. */
#include "ink/sys/appreg.h"
#include "ink/ui_shell.h"

static void create(lv_obj_t *screen, void *arg) { (void)arg; ink_shell_home_create(screen); }

const ink_activity_t ink_app_home = {
    .name = "home", .app = "home", .create = create, .destroy = ink_shell_home_destroy,
    .on_key = ink_shell_home_key, .on_swipe = ink_shell_home_swipe, .snapshot = ink_shell_home_snapshot
};
const ink_app_def_t ink_appdef_home = {
    .id = "home", .title = "主页", .entry = &ink_app_home, .system_app = true, .child_ok = true, .keep = true
};
