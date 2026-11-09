/* F6 shared presentation; UI task only. */
#ifndef INK_ACTIVITY_CARD_H
#define INK_ACTIVITY_CARD_H
#include "ink/ui_shell.h"
int ink_activity_card_init(void);
int ink_activity_card_draw(lv_obj_t *parent, int y);
bool ink_activity_home_draw(lv_obj_t *parent, int y);
int ink_activity_home_offset(void);
int ink_activity_card_snapshot(char *out, size_t cap);
bool ink_activity_listening(void);
void ink_shell_activity_refresh(void);
bool ink_phone_activity(char *title, size_t cap);
#endif
