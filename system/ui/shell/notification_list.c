/* ui-scale-runtime: dimension width Canvas width minus theme margins, shared by all shell pages. */
/* ui-scale-runtime: dimension y Rows accept theme positions. */
/* The control centre's notification rows, moved here unchanged from control_center.c (control-1); notify-ui-1 owns
 * this file from now on. */
#include "shell_internal.h"
#include "notification_list.h"

int ink_notification_list_draw(lv_obj_t *parent, int y, int limit) {
    ink_db_cursor_t cursor = {0};
    ink_notify_view_t n;
    int count = 0;
    ink_notify_view_t *visible = N.control_items;
    while (count < limit && ink_notify_next(&cursor, &n) == 1) {
        visible[count] = n;
        int h = ink_ui_row_double(),
            text_y = (h - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
        lv_obj_t *card = ink_ui_card(parent, INK_MARGIN, y, width(), h, INK_SURFACE_WHITE);
        ink_ui_circle(card, !strcmp(n.channel, "message") ? "message" : "bell", INK_GAP, (h - INK_CIRCLE) / 2,
                      INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL);
        ink_ui_line(card, n.title, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_ROW_DOUBLE, text_y,
                    width() - INK_ROW_DOUBLE - INK_MARGIN);
        ink_ui_line(card, n.body, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_ROW_DOUBLE,
                    text_y + ink_ui_line_height(INK_FONT_LIST), width() - INK_ROW_DOUBLE - INK_MARGIN);
        char args[64];
        snprintf(args, sizeof args, "{\"id\":%u}", (unsigned) n.id);
        ink_shell_bind(card, "shell.notify_open", args);
        count++;
        y += ink_ui_row_double() + INK_GAP_M;
    }
    if (!count) ink_ui_notice(parent, TR("还没有通知"), y + INK_GAP_L);
    N.control_count = count;
    return y;
}

int ink_notification_list_snapshot(char *buf, size_t cap, bool read) {
    int n = 0;
    for (int i = 0; i < N.control_count && n >= 0 && (size_t) n < cap; i++)
        n += read ? snprintf(buf + n, cap - (size_t) n, "通知[%u]=%s，%s；已读=%d；", (unsigned) N.control_items[i].id,
                             N.control_items[i].title, N.control_items[i].body, N.control_items[i].read)
                  : snprintf(buf + n, cap - (size_t)n, "通知[%u]=%s，%s；", (unsigned)N.control_items[i].id, N.control_items[i].title, N.control_items[i].body);
    return n;
}
