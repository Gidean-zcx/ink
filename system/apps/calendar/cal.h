/* calendar-3: the calendar app's pages share this (docs/tasks/calendar-3/对照表.md). */
#ifndef INK_CAL_H
#define INK_CAL_H
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/almanac.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

enum { CAL_ITEMS = 160, CAL_ROWS = 12, CAL_SLOTS = 8, CAL_FIELDS = 12, CAL_ACTIONS = 24 };

typedef struct { /* one agenda or day line: a date heading, an event's occurrence or an alarm */
    uint8_t kind;  /* 0 heading, 1 event, 2 alarm */
    uint32_t id;
    int64_t at, orig;
    char text[96], note[64];
} cal_item_t;

typedef struct { /* one row of a choices page */
    const char *label, *cap, *icon;
    char value[64], args[96];
    bool on, primary;
} cal_row_t;

typedef struct {
    char member[16];
    int y, m, d;            /* the selected day */
    int ty, tm, td;         /* today, as last drawn */
    uint8_t marks[42];      /* 1 an event without reminders, 2 one that rings */
    int32_t first;          /* local day of the grid's first cell */
    lv_obj_t *cells[42], *block, *block_lines[2], *undo_button;
    cal_item_t items[CAL_ITEMS];
    size_t count;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    char query[96], undo[96], undo_note[64], result[160];
    bool past, searched, undo_valid;
    /* the page's semantics */
    ink_ui_field_spec_t fields[CAL_FIELDS];
    ink_ui_action_spec_t actions[CAL_ACTIONS];
    ink_ui_page_spec_t page;
    char texts[CAL_FIELDS][192], args[CAL_ACTIONS][96], slot_ids[CAL_SLOTS][32], slot_args[CAL_SLOTS][64], slot_text[CAL_SLOTS][2][128];
    cal_row_t rows[CAL_ROWS];
    int nrows;
    lv_obj_t *field;
} cal_t;
extern cal_t K;

/* The event being edited (ReminderEdit and its pages). */
typedef struct {
    ink_event_t e;
    int64_t occurrence;     /* the original start of the occurrence opened, 0 for a new one */
    bool series, dirty, has_end, deleting;
    int part_target;        /* the picker: 0 start, 1 end, 2 until, 3 go to a date */
    ink_tm_t start, end, pick;
    char owner[16], error[160], at[32], end_text[32], object[32], step_value[24];
    int custom, custom_unit; /* a reminder being made: number and unit (0 minutes, 1 hours, 2 days) */
    lv_obj_t *name, *part_labels[INK_UI_DATETIME_PARTS], *step_label;
} cal_edit_t;
extern cal_edit_t E;

extern const ink_activity_t CAL_MONTH, CAL_AGENDA, CAL_DAY, CAL_ALMANAC, CAL_INFO, CAL_EDIT, CAL_TIME, CAL_REPEAT, CAL_DETAIL, CAL_END,
    CAL_REMINDERS, CAL_REMINDER_ADD, CAL_SCOPE, CAL_LEAVE, CAL_IMPORT, CAL_SETTINGS;

/* calendar.c */
int32_t cal_day(int y, int m, int d);
void cal_civil(int32_t day, int *y, int *m, int *d);
int cal_wday(int32_t day);
int cal_month_days(int y, int m);
bool cal_mine(void);
void cal_begin(lv_obj_t *screen, const char *title);
ink_ui_field_spec_t *cal_field(int hint, const char *id, const char *role, const char *label, const char *value);
ink_ui_action_spec_t *cal_action(int hint, const char *id, const char *label, const char *cap, const char *args);
int cal_snapshot(char *out, size_t cap);
void cal_lunar_short(int y, int m, int d, char *out, size_t cap);
void cal_lunar_full(int y, int m, int d, char *out, size_t cap);
const char *cal_weekday_name(int wday);
void cal_lead_text(int32_t lead, bool all_day, char *out, size_t cap);
void cal_rule_text(const ink_event_t *e, char *out, size_t cap);
bool cal_almanac(int y, int m, int d, ink_almanac_t *out, const char **why);
lv_obj_t *cal_pill(lv_obj_t *parent, const char *label, int x, int y, int w, bool primary, const char *cap, const char *args, int action);
lv_obj_t *cal_undo_button(lv_obj_t *parent, int right, int y, int action);
int cal_pill_w(const char *label);
bool cal_key(ink_key_t key, bool lng);
/* cal_month.c */
void cal_month_refresh(void *arg);
void cal_month_select(int y, int m, int d);
int cal_events_on(int32_t day, cal_item_t *items, int max);
/* cal_lists.c */
void cal_choices(lv_obj_t *screen, const char *title, const char *note, int y);
void cal_build_agenda(void);
/* cal_edit.c */
void cal_edit_open(const ink_event_t *e, int64_t occurrence, int y, int m, int d);
void cal_edit_cap(ink_call_t *call);
int cal_edit_init(void);
int cal_init(void);
#endif
