/* clock-3: what the clock's tabs and pages share (clock.c). UI task. */
#ifndef CLOCK_INTERNAL_H
#define CLOCK_INTERNAL_H
#include "clock_infinitime.h"
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/data.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/ui_shell.h"
#include <stdio.h>
#include <string.h>

enum { TAB_WORLD, TAB_ALARM, TAB_WATCH, TAB_TIMER, TABS };
/* One page model at a time (the top page's): what is drawn is declared here too, for touch and the AI alike. */
typedef struct {
    ink_ui_page_spec_t spec;
    ink_ui_field_spec_t f[40];
    ink_ui_action_spec_t a[48];
    lv_obj_t *name;   /* the page's name field, if it has one */
    char arena[12288];
    size_t used;
} ck_page_t;
extern ck_page_t CK;
typedef struct { char id[16]; uint8_t tab, sort; int8_t timer; unsigned page[TABS]; } ck_member_t;
extern ck_member_t *ck_me; /* the current member's slot (ck_member) */
extern lv_obj_t *ck_body;  /* the Clock page's content under the tabs */

void ck_begin(const char *id, const char *title, const char *object);
const char *ck_str(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int ck_field(const char *id, const char *label, const char *value);
void ck_text(lv_obj_t *label, const char *text, bool fast); /* only when it changed: only that label redraws */
void ck_value(int field, const char *value);
void ck_declare(const char *label, const char *cap, const char *args, bool on);
lv_obj_t *ck_btn(lv_obj_t *p, const char *label, const char *cap, const char *args, int x, int y, int w, int h, bool primary, bool on);
void ck_tap(lv_obj_t *o, const char *label, const char *cap, const char *args);
void ck_mark(lv_obj_t *button, bool on); /* a pill's chosen look (inverted), in place */
int ck_snapshot(char *out, size_t cap, void (*extra)(ink_jw_t *w));
int ck_member(void);
const char *ck_member_id(int m);
bool ck_on(const char *page);
unsigned ck_pager(lv_obj_t *p, int y, unsigned *page, unsigned total, unsigned per); /* the first index shown */
void ck_refresh(void);            /* the Clock page's tabs and content again, after a finger lifts */
const char *ck_day(int64_t utc_s); /* 今天, 明天, 昨天, 周一 (this week), 11月20日 */
const char *ck_wday(int wday);
const char *ck_next_alarm(bool left);
int ck_name(lv_obj_t *p, int y, const char *initial, const char *hint, size_t max, const char *focus_cap); /* its bottom */
void ck_focus(ink_call_t *c);
int ck_name_snapshot(char *out, size_t cap);
void ck_counter_row(lv_obj_t *p, int part, int y, const char *minus, const char *plus, const char *unit);
void ck_counter_label(int part);
/* the tabs */
void ck_world_body(int h);
void ck_alarm_body(int h);
void ck_watch_body(int h);
void ck_timer_body(int h);
void ck_world_extra(ink_jw_t *w);
void ck_alarm_extra(ink_jw_t *w);
void ck_watch_extra(ink_jw_t *w);
void ck_timer_extra(ink_jw_t *w);
void ck_world_minute(void);
void ck_alarm_minute(void);
bool ck_watch_tick(void);
bool ck_timer_ticks(void);
bool ck_watch_running(void);
bool ck_timer_running(void);
void ck_alarm_changed(void);
void ck_alarm_edit_label(int part);
void ck_timer_edit_label(int part);
int ck_timer_ring(int t);
void ck_timer_finished(int t);
void ck_timer_turn(int delta);
void ck_world_turn(int delta);
/* capability handlers and pages, by file */
void ck_alarm_cap(ink_call_t *c);
void ck_world_cap(ink_call_t *c);
void ck_watch_cap(ink_call_t *c);
void ck_timer_cap(ink_call_t *c);
int ck_world_init(void);
int ck_timer_init(void);
#endif
