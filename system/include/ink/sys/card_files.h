/*
 * Card files (D-138): what apps need on the storage card is put there by the system. The firmware carries a list
 * (made at build time from tools/card_files.json), checks the card at boot and when the card or the cable changes, and
 * fetches what is missing from the install tool on the USB cable (docs/system/card-files.md). An app page that cannot
 * open a file it needs says why with ink_card_files_notice, never a blank page.
 */
#ifndef INK_SYS_CARD_FILES_H
#define INK_SYS_CARD_FILES_H

#include <stddef.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INK_CARD_FILES_MAX 8
typedef enum { INK_CARD_READY, INK_CARD_CHECKING, INK_CARD_MISSING, INK_CARD_COPYING, INK_CARD_FAILED, INK_CARD_NO_CARD } ink_card_state_t;
int ink_card_files_init(void);                 /* states, capability, watches; before ink_ui_start */
void ink_card_files_start(void);               /* boot task, after ink_fs_refresh_state(): first check */
/* What the app needs: READY when every manifest entry of this app is in place; percent while copying. Any task. */
ink_card_state_t ink_card_files_state(const char *app, int *percent);
/* UI task: the explanation on an app's page while a file it needs is not usable. */
lv_obj_t *ink_card_files_notice(lv_obj_t *screen, const char *app, const char *what);
int ink_card_files_notice_snapshot(char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
