/* The control centre's notification rows (moved out of control_center.c by control-1; notify-ui-1 owns them from
 * here). UI task only. */
#ifndef INK_SHELL_NOTIFICATION_LIST_H
#define INK_SHELL_NOTIFICATION_LIST_H
#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"

/** The current member's newest notifications from y, at most `limit` rows (no more than N.control_items holds), each
 *  opening through shell.notify_open; what is drawn stays in N.control_items and N.control_count. Returns the y under
 *  the last row. */
int ink_notification_list_draw(lv_obj_t *parent, int y, int limit);
/** The rows drawn, for the page snapshot: "通知[id]=标题，正文；", and "已读=0/1；" after each when `read`. */
int ink_notification_list_snapshot(char *buf, size_t cap, bool read);

#endif
