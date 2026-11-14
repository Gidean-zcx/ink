/* Glue (ours, not LilyPhone): what the copied sms_view.c expects from its Arduino and LVGL 8 project, mapped onto this
 * system. sms.c implements the sms_*, scr_* and ui_sms_send functions over the member's sms.messages. */
#ifndef SMS_LILY_H
#define SMS_LILY_H
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "ink/sys/app.h"
#include "ink/sys/comm.h"
#include "ink/sys/i18n.h"
#include "ink/sys/time.h"
#include "ink/ui_theme.h"

/* include/phone_store.h, with this system's sizes */
#define CONTACT_NAME_LEN 97
#define CONTACT_NUMBER_LEN 25
#define SMS_TEXT_LEN INK_SMS_TEXT_MAX
#define SMS_DIR_IN 0
#define SMS_DIR_OUT 1
#define SMS_ST_OK 0
#define SMS_ST_PENDING 1
#define SMS_ST_FAILED 2
#define SMS_ST_UNKNOWN 3 /* ours: the service cannot tell whether it went */
#define SMS_ST_DRAFT 4   /* ours: a conversation's unsent text */
typedef struct {
    char number[CONTACT_NUMBER_LEN];
    char text[SMS_TEXT_LEN];
    uint32_t ts;
    uint8_t dir, status, unread;
} sms_msg_t;
bool phone_number_match(const char *a, const char *b); /* phone-3's copy (system/apps/phone/vendor/lilyphone) */

/* the log and its conversations: sms.c's index of the member's sms.messages, in PSRAM */
const char *sms_active_number(void);
#define ui_active_number sms_active_number()
uint32_t ui_sms_send(const char *number, const char *text); /* message.send; 0 when it could not be asked */
int sms_add(const char *number, const char *text, uint32_t ts, int dir, int status, bool unread);
bool sms_delete(int idx);
void sms_thread_delete(const char *number);
int sms_thread_count(void);
const char *sms_thread_number(int thread);
const sms_msg_t *sms_thread_last(int thread);
int sms_thread_unread(const char *number);
int sms_thread_msg_count(const char *number);
const char *sms_display_name(const char *number); /* the contact's name, also for +86 / 0086 numbers */
#define contacts_display_name sms_display_name
extern uint32_t ui_send_watch_id, ui_sms_revision, scr13_built_revision;
extern int ui_send_watch_idx;
typedef struct { char emoji[8]; const char *word; char quoted[32]; bool removal; } ui_reaction_t;
static inline bool ui_reaction_parse(const char *text, ui_reaction_t *out) { (void)text; memset(out, 0, sizeof *out); return false; } /* no reactions */

/* its screens: our pages (sms.c) */
extern lv_obj_t *scr13_2_body_ta, *scr13_2_status; /* the shared reply field and the line above it */
#define lv_textarea_get_text(ta) sms_field_text(ta)
const char *sms_field_text(lv_obj_t *field);
#define lv_textarea_set_text(ta, text) sms_field_set(text) /* the shared field has no setter: rebuilt */
void sms_field_set(const char *text);
void lily_sent(void);
#define scr_mgr_pop(anim) sms_pop()
void sms_pop(void);
void scr13_1_populate(void); /* the open conversation, laid out again in place */
void scr13_1_bubble_event(lv_event_t *e); /* sms.select */
#define scr13_list NULL
#define scr13_row_event NULL
#define lv_obj_clean(list) ((void)(list))
#define scr_empty_note_create(list, text) ((void)(list)) /* ink_ui_list's own empty state */
void scr_row_create(lv_obj_t *list, const char *title, const char *snippet, const char *badge, lv_event_cb_t cb, void *ctx);
const char *ink_textfield_text(lv_obj_t *field);

/* its theme: our tokens (text grows with the system text size) */
#define FONT_BOLD_SIZE_15 ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR)
#define DECKPRO_COLOR_FG ink_ui_ink()
#define DECKPRO_COLOR_BG ink_ui_paper()
#define DECKPRO_COLOR_SHADE ink_ui_separator() /* received: a light grey ground (30 % of the separator) */
#define SMS_CONTENT_W (INK_CANVAS_W - 2 * INK_MARGIN)
#define SMS_BUBBLE_W (SMS_CONTENT_W * 4 / 5)

/* its clock: the system's local time */
#define time(t) ((time_t)ink_time_now_s())
static inline struct tm *lily_localtime_r(const time_t *t, struct tm *out) {
    static const int days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    ink_tm_t l; ink_time_to_local((int64_t)*t, &l);
    *out = (struct tm){.tm_year = l.year - 1900, .tm_mon = l.month - 1, .tm_mday = l.day, .tm_hour = l.hour, .tm_min = l.min, .tm_sec = l.sec, .tm_wday = l.wday,
        .tm_yday = days[(l.month - 1) % 12] + l.day - 1 + (l.month > 2 && l.year % 4 == 0 && (l.year % 100 || l.year % 400 == 0))};
    return out;
}
#define localtime_r lily_localtime_r

/* sms_view.c's own entry points for sms.c */
bool lily_dispatch(const char *number, const char *text);
void lily_snippet(char *buf, int len, const char *text);
void lily_stamp(char *buf, int len, uint32_t ts, bool compact);
void lily_rows(void);
void lily_delete(int idx);
void lily_delete_thread(void);
lv_coord_t lily_bubble(lv_obj_t *parent, const sms_msg_t *m, int id, lv_coord_t y);
void lily_send(void);
#endif
