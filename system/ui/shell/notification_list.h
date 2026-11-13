/* The control centre's notification rows, moved out of control_center.c by control-1; notify-ui-1 owns them. UI task. */
#ifndef INK_SHELL_NOTIFICATION_LIST_H
#define INK_SHELL_NOTIFICATION_LIST_H
#include <stdbool.h>
#include "lvgl.h"
/** At most `limit` (N.control_items holds 2) of the member's newest notifications as rows from y, each opening through
 *  shell.notify_open; kept in N.control_items / N.control_count. Nothing when there are none. Returns the y under them. */
int ink_notification_list_draw(lv_obj_t *parent, int y, int limit);
/** Those rows for the page snapshot: "通知[id]=标题，正文；", with "已读=0/1；" when `read`. */
int ink_notification_list_snapshot(char *buf, size_t cap, bool read);
#endif
