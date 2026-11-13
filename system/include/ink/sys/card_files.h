/* Card files (D-138, docs/system/card-files.md): the system puts what apps need on the storage card. */
#ifndef INK_SYS_CARD_FILES_H
#define INK_SYS_CARD_FILES_H
#include "lvgl.h"
typedef enum { INK_CARD_READY, INK_CARD_CHECKING, INK_CARD_MISSING, INK_CARD_COPYING, INK_CARD_FAILED, INK_CARD_NO_CARD } ink_card_state_t;
int ink_card_files_init(void); /* states, capability, watches; posts the first check to exec.bg. Before ink_ui_start */
ink_card_state_t ink_card_files_state(const char *app, int *percent); /* any task: READY when app has all its files */
unsigned ink_card_files_checks(void); /* any task: how many checks (and copies) have come to a conclusion since boot */
/* UI task: a page that cannot open a file it needs says why and holds none of its files meanwhile (they may be
 * replaced); reopen() is called once they are in place (NULL: the page is redrawn). Snapshot: what it says, 0: none. */
lv_obj_t *ink_card_files_notice(lv_obj_t *screen, const char *app, const char *what, void (*reopen)(void));
int ink_card_files_notice_snapshot(const char *app, char *out, size_t cap);
#endif
