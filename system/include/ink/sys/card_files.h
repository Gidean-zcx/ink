/* Card files (D-138, docs/system/card-files.md): what apps need on the storage card is put there by the system, from the
 * list built into the firmware. A page that cannot open such a file says why with ink_card_files_notice. */
#ifndef INK_SYS_CARD_FILES_H
#define INK_SYS_CARD_FILES_H
#include <stddef.h>
#include "lvgl.h"
#define INK_CARD_FILES_MAX 8
typedef enum { INK_CARD_READY, INK_CARD_CHECKING, INK_CARD_MISSING, INK_CARD_COPYING, INK_CARD_FAILED, INK_CARD_NO_CARD } ink_card_state_t;
int ink_card_files_init(void);   /* states, capability, watches; before ink_ui_start */
void ink_card_files_start(void); /* boot task, after ink_fs_refresh_state(): first check */
/* What the app needs: READY when every manifest entry of this app is in place; percent while copying. Any task. */
ink_card_state_t ink_card_files_state(const char *app, int *percent);
/* UI task: the explanation on an app's page while a file it needs is not usable; and what it says, for the AI. */
lv_obj_t *ink_card_files_notice(lv_obj_t *screen, const char *app, const char *what);
int ink_card_files_notice_snapshot(char *out, size_t cap);
#endif
