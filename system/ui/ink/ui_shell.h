/* Shell presentation and navigation; all functions run on the UI task. */
#ifndef INK_UI_SHELL_H
#define INK_UI_SHELL_H
#include "ink/ui_theme.h"
#include "ink/sys/ai.h"
#include "ink/sys/cap.h"

int ink_shell_init(void);
int ink_kid_pages_init(void);
void ink_shell_kid_limit(const char *app);
void ink_shell_kid_close(void);
int ink_settings_pages_init(void);
void ink_settings_page_open(const char *name);
int ink_boot_flow_init(void);
void ink_boot_flow_resume(void);
bool ink_boot_flow_launch(ink_call_t *call);
void ink_ui_stage_next(void);
const char *ink_ui_action_page(void);
void ink_shell_keyboard_area(lv_area_t *area);
int ink_ui_context_snapshot(char *buf, size_t cap, const char **name);
void ink_shell_home_create(lv_obj_t *screen);
void ink_shell_home_destroy(void);
bool ink_shell_home_swipe(ink_swipe_dir_t dir);
bool ink_shell_home_key(ink_key_t key, bool long_press);
int ink_shell_home_snapshot(char *buf, size_t cap);
void ink_shell_control(void);
bool ink_shell_close(void);
lv_obj_t *ink_shell_statusbar(lv_obj_t *parent, bool show_time);
void ink_shell_time_strings(char *clock, size_t clock_cap, char *date, size_t date_cap);
lv_obj_t *ink_shell_dock(lv_obj_t *parent);
void ink_shell_topbar(lv_obj_t *parent, const char *title);
void ink_shell_bind(lv_obj_t *obj, const char *cap, const char *json);
const char *ink_shell_bound_cap(lv_obj_t *obj);
void ink_shell_open_page(const char *name);
const char *ink_shell_board(void);
void ink_shell_members_open(bool returning);
void ink_shell_ai_open(lv_obj_t **root, lv_obj_t **status, lv_obj_t **question, lv_obj_t **answer);
void ink_shell_ai_closed(void);
void ink_shell_ai_status(const char *status, ink_ai_error_t error);
void ink_shell_ai_words(const char *text);
bool ink_shell_ai_hide(void);
void ink_shell_ai_reveal(void);
void ink_ai_tap(void);
int ink_shell_ai_snapshot(char *buf, size_t cap);

#endif
